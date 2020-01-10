#include <omnetpp.h>
#include <stdlib.h>

using namespace omnetpp;


class Source : public cSimpleModule
{
  private:
    cMessage *sendMessageEvent;
    int nbGenMessages;
    int priority;

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
    priority = 0;
    nbGenMessages = 0;
}

void Source::handleMessage(cMessage *msg)
{
    ASSERT(msg == sendMessageEvent);
    int n=par("numberofpriority");
    priority = rand()%n+1;
    char msgname[20];
    sprintf(msgname,"message-%d", ++nbGenMessages);
    cMessage *message = new cMessage(msgname);
    message->setSchedulingPriority(priority);
    send(message, "out");
    char param[20];
    sprintf(param, "interArrivalTime%d", priority);
    scheduleAt(simTime()+par(param).doubleValue(), sendMessageEvent);
}
