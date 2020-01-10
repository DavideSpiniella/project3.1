#include <omnetpp.h>
#include <stdlib.h>

using namespace omnetpp;


class Source : public cSimpleModule
{
  private:
    cMessage *sendMessageEvent;
    int nbGenMessages;

  public:
    Source();
    virtual ~Source();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};

Define_Module(Source);

Source::Source()
{
    sendMessageEvent = nullptr;
}

Source::~Source()
{
    cancelAndDelete(sendMessageEvent);
}

void Source::initialize()
{
    sendMessageEvent = new cMessage("sendMessageEvent");
    scheduleAt(simTime(), sendMessageEvent);
}

void Source::handleMessage(cMessage *msg)
{
    ASSERT(msg == sendMessageEvent);
    int n=par("numberofpriority");
    char msgname[20];
    sprintf(msgname,"message-%d", ++nbGenMessages);
    cMessage *message = new cMessage(msgname);
    message->setSchedulingPriority(rand()%n+1);
    send(message, "out");

    scheduleAt(simTime()+par("interArrivalTime").doubleValue(), sendMessageEvent);
}
