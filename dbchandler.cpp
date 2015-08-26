#include "dbchandler.h"

#include <QFile>
#include <QRegularExpression>
#include <QDebug>

DBCHandler::DBCHandler(QObject *parent) : QObject(parent)
{

}

void DBCHandler::loadDBCFile(QString filename)
{
    QFile *inFile = new QFile(filename);
    QString line;
    QRegularExpression regex;
    QRegularExpressionMatch match;
    DBC_MESSAGE *currentMessage;

    qDebug() << "DBC File: " << filename;

    if (!inFile->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete inFile;
        return;
    }

    qDebug() << "Starting DBC load";
    dbc_nodes.clear();
    dbc_messages.clear();

    DBC_NODE falseNode;
    falseNode.name = "Vector__XXX";
    falseNode.comment = "Default node if none specified";
    dbc_nodes.append(falseNode);

    while (!inFile->atEnd()) {
        line = QString(inFile->readLine().simplified());
        if (line.startsWith("BO_ "))
        {
            //qDebug() << "Found a BO line";
            regex.setPattern("^BO\\_ (\\w+) (\\w+) *: (\\w+) (\\w+)");
            match = regex.match(line);
            //captured 1 = the ID in decimal
            //captured 2 = The message name
            //captured 3 = the message length
            //captured 4 = the NODE responsible for this message
            if (match.hasMatch())
            {
                DBC_MESSAGE msg;
                msg.ID = match.captured(1).toInt(); //the ID is always stored in decimal format
                msg.name = match.captured(2);
                msg.len = match.captured(3).toInt();
                msg.sender = findNodeByName(match.captured(4));
                dbc_messages.append(msg);
                currentMessage = &dbc_messages.last();
            }
        }
        if (line.startsWith("SG_ "))
        {
            //qDebug() << "Found a SG line";
            regex.setPattern("^SG\\_ (\\w+) : (\\d+)\\|(\\d+)@(\\d+)([\\+|\\-]) \\(([0-9.+\\-eE]+),([0-9.+\\-eE]+)\\) \\[([0-9.+\\-eE]+)\\|([0-9.+\\-eE]+)\\] \\\"(.*)\\\" (.*)");
            match = regex.match(line);
            //captured 1 is the signal name
            //captured 2 is the starting bit
            //captured 3 is the length in bits
            //captured 4 is the byte order / value type
            //captured 5 specifies signed/unsigned for ints
            //captured 6 is the scaling factor
            //captured 7 is the offset
            //captured 8 is the minimum value
            //captured 9 is the maximum value
            //captured 10 is the unit
            //captured 11 is the receiving node

            if (match.hasMatch())
            {
                DBC_SIGNAL sig;
                sig.name = match.captured(1);
                sig.startBit = match.captured(2).toInt();
                sig.signalSize = match.captured(3).toInt();
                int val = match.captured(4).toInt();
                if (val < 2)
                {
                    if (match.captured(5) == "+") sig.valType = UNSIGNED_INT;
                    else sig.valType = SIGNED_INT;
                }
                switch (val)
                {
                case 0: //big endian mode
                    sig.intelByteOrder = false;
                    break;
                case 1: //little endian mode
                    sig.intelByteOrder = true;
                    break;
                case 2:
                    sig.valType = SP_FLOAT;
                    break;
                case 3:
                    sig.valType = DP_FLOAT;
                    break;
                case 4:
                    sig.valType = STRING;
                    break;
                }
                sig.factor = match.captured(6).toDouble();
                sig.bias = match.captured(7).toDouble();
                sig.min = match.captured(8).toDouble();
                sig.max = match.captured(9).toDouble();
                sig.unitName = match.captured(10);
                if (match.captured(11).contains(','))
                {
                    QString tmp = match.captured(11).split(',')[0];
                    sig.receiver = findNodeByName(tmp);
                }
                else sig.receiver = findNodeByName(match.captured(11));
                currentMessage->msgSignals.append(sig);
            }
        }
        if (line.startsWith("BU_:"))
        {
            //qDebug() << "Found a BU line";
            regex.setPattern("^BU\\_\\:(.*)");
            match = regex.match(line);
            //captured 1 = a list of node names separated by spaces. No idea how many yet
            if (match.hasMatch())
            {
                QStringList nodeStrings = match.captured(1).split(' ');
                qDebug() << "Found " << nodeStrings.count() << " node names";
                for (int i = 0; i < nodeStrings.count(); i++)
                {
                    //qDebug() << nodeStrings[i];
                    if (nodeStrings[i].length() > 1)
                    {
                        DBC_NODE node;
                        node.name = nodeStrings[i];
                        dbc_nodes.append(node);
                    }
                }
            }
        }
        if (line.startsWith("CM_ SG_ "))
        {
            //qDebug() << "Found an SG comment line";
            regex.setPattern("^CM\\_ SG\\_ *(\\w+) *(\\w+) *\\\"(.*)\\\";");
            match = regex.match(line);
            //captured 1 is the ID to match against to get to the message
            //captured 2 is the signal name from that message
            //captured 3 is the comment itself
            if (match.hasMatch())
            {
                //qDebug() << "Comment was: " << match.captured(3);
                DBC_MESSAGE *msg = findMsgByID(match.captured(1).toInt());
                if (msg != NULL)
                {
                    DBC_SIGNAL *sig = findSignalByName(msg, match.captured(2));
                    if (sig != NULL)
                    {
                        sig->comment = match.captured(3);
                    }
                }
            }
        }
        if (line.startsWith("CM_ BO_ "))
        {
            //qDebug() << "Found a BO comment line";
            regex.setPattern("^CM\\_ BO\\_ *(\\w+) *\\\"(.*)\\\";");
            match = regex.match(line);
            //captured 1 is the ID to match against to get to the message
            //captured 2 is the comment itself
            if (match.hasMatch())
            {
                //qDebug() << "Comment was: " << match.captured(2);
                DBC_MESSAGE *msg = findMsgByID(match.captured(1).toInt());
                if (msg != NULL)
                {
                    msg->comment = match.captured(2);
                }
            }
        }
        if (line.startsWith("CM_ BU_ "))
        {
            //qDebug() << "Found a BU comment line";
            regex.setPattern("^CM\\_ BU\\_ *(\\w+) *\\\"(.*)\\\";");
            match = regex.match(line);
            //captured 1 is the Node name
            //captured 2 is the comment itself
            if (match.hasMatch())
            {
                //qDebug() << "Comment was: " << match.captured(2);
                DBC_NODE *node = findNodeByName(match.captured(1));
                if (node != NULL)
                {
                    node->comment = match.captured(2);
                }
            }
        }
        //VAL_ (1090) (VCUPresentParkLightOC) (1 "Error present" 0 "Error not present") ;
        if (line.startsWith("VAL_ "))
        {
            //qDebug() << "Found a value definition line";
            regex.setPattern("^VAL\\_ (\\w+) (\\w+) (.*);");
            match = regex.match(line);
            //captured 1 is the ID to match against
            //captured 2 is the signal name to match against
            //captured 3 is a series of values in the form (number "text") that is, all sep'd by spaces
            if (match.hasMatch())
            {
                //qDebug() << "Data was: " << match.captured(3);
                DBC_MESSAGE *msg = findMsgByID(match.captured(1).toInt());
                if (msg != NULL)
                {
                    DBC_SIGNAL *sig = findSignalByName(msg, match.captured(2));
                    if (sig != NULL)
                    {
                        QString tokenString = match.captured(3);
                        DBC_VAL val;
                        while (tokenString.length() > 2)
                        {
                            regex.setPattern("(\\d+) \\\"(.*?)\\\" (.*)");
                            match = regex.match(tokenString);
                            if (match.hasMatch())
                            {
                                val.value = match.captured(1).toInt();
                                val.descript = match.captured(2);
                                //qDebug() << "sig val " << val.value << " desc " <<val.descript;
                                sig->valList.append(val);
                                tokenString = tokenString.right(tokenString.length() - match.captured(1).length() - match.captured(2).length() - 4);
                                //qDebug() << "New token string: " << tokenString;
                            }
                            else tokenString = "";
                        }
                    }
                }
            }
        }

        /*
        if (line.startsWith("BA_DEF_ SG_ "))
        {
            qDebug() << "Found a SG attribute line";
            regex.setPattern("^BA\\_DEF\\_ SG\\_ +\\\"([A-Za-z0-9\-_]+)\\\" +(.+);");
            match = regex.match(line);
            //captured 1 is the Node name
            //captured 2 is the comment itself
            if (match.hasMatch())
            {
                qDebug() << "Comment was: " << match.captured(2);
            }
        }
        if (line.startsWith("BA_DEF_ BO_ "))
        {

        }
        if (line.startsWith("BA_DEF_ BU_ "))
        {

        }
*/
    }
    inFile->close();
    delete inFile;
}

/*Yes, this is really hard to follow and all of the sections are mixed up in code
 * believe it or not I think this is actually the easiest, simplest way to do it.
*/
void DBCHandler::saveDBCFile(QString filename)
{
    QFile *outFile = new QFile(filename);
    QString nodesOutput, msgOutput, commentsOutput, valuesOutput;

    if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        delete outFile;
        return;
    }

    //right now it outputs a standard hard coded boilerplate
    outFile->write("VERSION \"\"\n");
    outFile->write("\n");
    outFile->write("\n");
    outFile->write("NS_ :\n");
    outFile->write("    NS_DESC_\n");
    outFile->write("    CM_\n");
    outFile->write("    BA_DEF_\n");
    outFile->write("    BA_\n");
    outFile->write("    VAL_\n");
    outFile->write("    CAT_DEF_\n");
    outFile->write("    CAT_\n");
    outFile->write("    FILTER\n");
    outFile->write("    BA_DEF_DEF_\n");
    outFile->write("    EV_DATA_\n");
    outFile->write("    ENVVAR_DATA_\n");
    outFile->write("    SGTYPE_\n");
    outFile->write("    SGTYPE_VAL_\n");
    outFile->write("    BA_DEF_SGTYPE_\n");
    outFile->write("    BA_SGTYPE_\n");
    outFile->write("    SIG_TYPE_REF_\n");
    outFile->write("    VAL_TABLE_\n");
    outFile->write("    SIG_GROUP_\n");
    outFile->write("    SIG_VALTYPE_\n");
    outFile->write("    SIGTYPE_VALTYPE_\n");
    outFile->write("    BO_TX_BU_\n");
    outFile->write("    BA_DEF_REL_\n");
    outFile->write("    BA_REL_\n");
    outFile->write("    BA_DEF_DEF_REL_\n");
    outFile->write("    BU_SG_REL_\n");
    outFile->write("    BU_EV_REL_\n");
    outFile->write("    BU_BO_REL_\n");
    outFile->write("    SG_MUL_VAL_\n");
    outFile->write("\n");
    outFile->write("BS_: \n");

    nodesOutput.append("BU_: ");
    for (int x = 0; x < dbc_nodes.count(); x++)
    {
        DBC_NODE node = dbc_nodes[x];
        if (node.name.compare("Vector__XXX", Qt::CaseInsensitive) != 0)
        {
            nodesOutput.append(node.name + " ");
            if (node.comment.length() > 0)
            {
                commentsOutput.append("CM_ BU_ " + node.name + " \"" + node.comment + "\";\n");
            }
        }
    }
    nodesOutput.append("\n");
    outFile->write(nodesOutput.toUtf8());

    for (int x = 0; x < dbc_messages.count(); x++)
    {
        DBC_MESSAGE msg = dbc_messages[x];
        msgOutput.append("BO_ " + QString::number(msg.ID) + " " + msg.name + ": " + QString::number(msg.len) +
                         " " + msg.sender->name + "\n");
        if (msg.comment.length() > 0)
        {
            commentsOutput.append("CM_ BO_ " + QString::number(msg.ID) + " \"" + msg.comment + "\";\n");
        }
        for (int s = 0; s < msg.msgSignals.count(); s++)
        {
            DBC_SIGNAL sig = msg.msgSignals[s];
            msgOutput.append("    SG_ " + sig.name + " : " + QString::number(sig.startBit) + "|" + QString::number(sig.signalSize) + "@");
            switch (sig.valType)
            {
            case UNSIGNED_INT:
                if (sig.intelByteOrder) msgOutput.append("1+");
                else msgOutput.append("0+");
                break;
            case SIGNED_INT:
                if (sig.intelByteOrder) msgOutput.append("1-");
                else msgOutput.append("0-");
                break;
            case SP_FLOAT:
                msgOutput.append("2-");
                break;
            case DP_FLOAT:
                msgOutput.append("3-");
                break;
            case STRING:
                msgOutput.append("4-");
                break;
            default:
                msgOutput.append("0-");
                break;
            }
            msgOutput.append(" (" + QString::number(sig.factor) + "," + QString::number(sig.bias) + ") [" +
                             QString::number(sig.min) + "|" + QString::number(sig.max) + "] \"" + sig.unitName
                             + "\" " + sig.receiver->name + "\n");
            if (sig.comment.length() > 0)
            {
                commentsOutput.append("CM_ SG_ " + QString::number(msg.ID) + " " + sig.name + " \"" + sig.comment + "\";\n");
            }
            if (sig.valList.count() > 0)
            {
                valuesOutput.append("VAL_ " + QString::number(msg.ID) + " " + sig.name);
                for (int v = 0; v < sig.valList.count(); v++)
                {
                    DBC_VAL val = sig.valList[v];
                    valuesOutput.append(" " + QString::number(val.value) + " \"" + val.descript +"\"");
                }
                valuesOutput.append(";\n");
            }
        }
        msgOutput.append("\n");
        //write it out every message so the string doesn't end up too huge
        outFile->write(msgOutput.toUtf8());
        msgOutput.clear(); //got to reset it after writing
    }

    //now write out all of the accumulated comments and value tables from above
    outFile->write(commentsOutput.toUtf8());
    outFile->write(valuesOutput.toUtf8());

    outFile->close();
    delete outFile;
}


DBC_NODE *DBCHandler::findNodeByName(QString name)
{
    if (dbc_nodes.length() == 0) return NULL;
    for (int i = 0; i < dbc_nodes.length(); i++)
    {
        if (dbc_nodes[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return &dbc_nodes[i];
        }
    }
    return NULL;
}

DBC_NODE *DBCHandler::findNodeByIdx(int idx)
{
    if (idx < 0) return NULL;
    if (idx >= dbc_nodes.count()) return NULL;
    return &dbc_nodes[idx];
}

DBC_MESSAGE *DBCHandler::findMsgByID(int id)
{
    if (dbc_messages.length() == 0) return NULL;
    for (int i = 0; i < dbc_messages.length(); i++)
    {
        if (dbc_messages[i].ID == id)
        {
            return &dbc_messages[i];
        }
    }
    return NULL;
}

DBC_MESSAGE *DBCHandler::findMsgByIdx(int idx)
{
    if (dbc_messages.length() == 0) return NULL;
    if (idx < 0) return NULL;
    if (idx >= dbc_messages.count()) return NULL;
    return &dbc_messages[idx];
}

DBC_MESSAGE *DBCHandler::findMsgByName(QString name)
{
    if (dbc_messages.length() == 0) return NULL;
    for (int i = 0; i < dbc_messages.length(); i++)
    {
        if (dbc_messages[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return &dbc_messages[i];
        }
    }
    return NULL;
}

DBC_SIGNAL *DBCHandler::findSignalByName(DBC_MESSAGE *msg, QString name)
{
    if (msg == NULL) return NULL;
    if (msg->msgSignals.length() == 0) return NULL;
    for (int i = 0; i < msg->msgSignals.length(); i++)
    {
        if (msg->msgSignals[i].name.compare(name, Qt::CaseInsensitive) == 0)
        {
            return &msg->msgSignals[i];
        }
    }
    return NULL;
}

DBC_SIGNAL *DBCHandler::findSignalByIdx(DBC_MESSAGE *msg, int idx)
{
    if (msg == NULL) return NULL;
    if (msg->msgSignals.length() == 0) return NULL;
    if (idx < 0) return NULL;
    if (idx >= msg->msgSignals.count()) return NULL;
    return &msg->msgSignals[idx];
}

//Dumps the messages, signals, values structs out in order to debugging console. Used only for debugging
//not really meant for general consumption.
void DBCHandler::listDebugging()
{
    for (int i = 0; i < dbc_messages.length(); i++)
    {
        DBC_MESSAGE msg = dbc_messages.at(i);
        qDebug() << " ";
        qDebug() << "Msg ID: " << msg.ID << " Name: " << msg.name;

        for (int j = 0; j < msg.msgSignals.length(); j++)
        {
            DBC_SIGNAL sig;
            sig = msg.msgSignals.at(j);
            qDebug() << "    Signal Name: " << sig.name;
            qDebug() << "    Start bit: " << sig.startBit;
            qDebug() << "    Bit Length: " << sig.signalSize;
            if (sig.valList.length() > 1) qDebug() << "      Values: ";
            for (int k = 0; k < sig.valList.length(); k++)
            {
                DBC_VAL val = sig.valList.at(k);
                qDebug() << "            " << val.value << " Description: " << val.descript;
            }
        }
    }
}


/*
 The way that the DBC file format works is kind of weird... For intel format signals you count up
from the start bit to the end bit which is (startbit + signallength - 1). At each point
bits are numbered in a sawtooth manner. What that means is that the very first bit is 0 and you count up
from there all of the way to 63 with each byte being 8 bits so bit 0 is the lowest bit in the first byte
and 8 is the lowest bit in the next byte up. The whole thing looks like this:
                 Bits
      7  6  5  4  3  2  1  0

  0   7  6  5  4  3  2  1  0
b 1   15 14 13 12 11 10 9  8
y 2   23 22 21 20 19 18 17 16
t 3   31 30 29 28 27 26 25 24
e 4   39 38 37 36 35 34 33 32
s 5   47 46 45 44 43 42 41 40
  6   55 54 53 52 51 50 49 48
  7   63 62 61 60 59 58 57 56

  For intel format you start at the start bit and keep counting up. If you have a signal size of 8
  and start at bit 12 then the bits are 12, 13, 14, 15, 16, 17, 18, 19 which spans across two bytes.
  In this format each bit is worth twice as much as the last and you just keep counting up.
  Bit 12 is worth 1, 13 is worth 2, 14 is worth 4, etc all of the way to bit 19 is worth 128.

  Motorola format turns most everything on its head. You count backward from the start bit but
  only within the current byte. If you are about to exit the current byte you go one higher and then keep
  going backward as before. Using the same example as for intel, start bit of 12 and a signal length of 8.
  So, the bits are 12, 11, 10, 9, 8, 23, 22, 21. Yes, that's confusing. They now go in reverse value order too.
  Bit 12 is worth 128, 11 is worth 64, etc until bit 21 is worth 1.
*/

QString DBCHandler::processSignal(const CANFrame &frame, const DBC_SIGNAL &sig)
{

    int64_t result = 0;
    int bit;
    int sigSize;
    if (sig.valType == STRING)
    {
        QString buildString;
        int startByte = sig.signalSize / 8;
        int bytes = sig.signalSize / 8;
        for (int x = 0; x < bytes; x++) buildString.append(frame.data[startByte + x]);
        return buildString;
    }

    sigSize = sig.signalSize;

    if (sig.intelByteOrder)
    {
        bit = sig.startBit;
        for (int bitpos = 0; bitpos < sigSize; bitpos++)
        {
            if (frame.data[bit / 8] & (1 << (bit % 8)))
                result += (1ULL << bitpos);

            bit++;
        }
    }
    else //motorola / big endian mode
    {
        bit = sig.startBit;
        for (int bitpos = 0; bitpos < sigSize; bitpos++)
        {
            if (frame.data[bit / 8] & (1 << (bit % 8)))
                result += (1ULL << (sigSize - bitpos - 1));

            if ((bit % 8) == 0)
                bit += 15;
            else bit--;

        }
    }

    if (sig.valType == SIGNED_INT)
    {
        int mask = (1 << (sig.signalSize - 1));
        if ((result & mask) == mask) //is the highest bit possible for this signal size set?
        {
            /*
             * if so we need to also set every bit higher in the result int too.
             * This leads to the below two lines that are nasty. Here's the theory behind that...
             * If the value is signed and the highest bit is set then it is negative. To create
             * a negative value out of this even though the variable result is 64 bit we have to
             * run 1's all of the way up to bit 63 in result. -1 is all ones for whatever size integer
             * you have. So, it's 64 1's in this case.
             * signedMask is done this way:
             * first you take the signal size and shift 1 up that far. Then subtract one. Lets
             * see that for a 16 bit signal:
             * (1 << 16) - 1 = the first 16 bits set as 1's. So far so good. We then negate the whole
             * thing which flips all bits. Thus signedMask ends up with 1's everwhere that the signal
             * doesn't take up in the 64 bit signed integer result. Then, result has an OR operation on
             * it with the old value and -1 masked so that the the 1 bits from -1 don't overwrite bits from the
             * actual signal. This extends the sign bits out so that the integer result reads as the proper negative
             * value. We dont need to do any of this if the sign bit wasn't set.
            */
            int signedMask = ~((1 << sig.signalSize) - 1);
            result = (-1 & signedMask) | result;
        }
    }

    double endResult = ((double)result * sig.factor) + sig.bias;
    result = (int) endResult;

    QString outputString;

    outputString = sig.name + ": ";

    if (sig.valList.count() > 0) //if this is a value list type then look it up and display the proper string
    {
        for (int x = 0; x < sig.valList.count(); x++)
        {
            if (sig.valList.at(x).value == result) outputString += sig.valList.at(x).descript;
        }
    }
    else //otherwise display the actual number and unit (if it exists)
    {
       outputString += QString::number(endResult) + sig.unitName;
    }

    return outputString;
}

//given a byte it will reverse the bit order in that byte
unsigned char DBCHandler::reverseBits(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

unsigned char DBCHandler::processByte(unsigned char input, int start, int end)
{
    unsigned char output = 0, size = end - start + 1;
    //first knock it down so that bottom is is start
    output = input >> start;
    //then mask off all bits above the proper ending
    output &= ((1 << size) - 1);
    return output;
}
