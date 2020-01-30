#include <omnetpp.h>
#include <stdlib.h>

using namespace omnetpp;


class Source : public cSimpleModule
{
  private:
    cMessage *sendMessageEvent;
    int nbGenMessages;
    int schedulingClasses;
    int numberofSchedulingClasses;
    // scelgo casualmente uno degli arrival time disponibili
    int * arrivalTime;

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
    schedulingClasses = 0;
    nbGenMessages = 0;
    // prelevo il parametro
    numberofSchedulingClasses=par("numberofSchedulingClasses");
    // così dichiaro l'array con una posizione per ogni classe
    arrivalTime = (int*) malloc(sizeof(int) * numberofSchedulingClasses);
    // per ogni classe scelgo casualmente uno degli arrival time disponibili
    for (int i = 1; i <= numberofSchedulingClasses ; i++){
        arrivalTime[i] = rand()%5+1;
    }
}

void Source::handleMessage(cMessage *msg)
{
    ASSERT(msg == sendMessageEvent);
    // scelgo casualmente una delle classi disponibili
    schedulingClasses = rand()%numberofSchedulingClasses+1;
    char msgname[20];
    sprintf(msgname,"message-%d", ++nbGenMessages);
    cMessage *message = new cMessage(msgname);
    message->setSchedulingPriority(schedulingClasses);
    send(message, "out");
    char param[20];
    sprintf(param, "interArrivalTime%d", arrivalTime[schedulingClasses]);
    scheduleAt(simTime()+par(param).doubleValue(), sendMessageEvent);
}
