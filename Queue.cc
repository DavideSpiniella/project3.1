#include <omnetpp.h>

using namespace omnetpp;


class Queue : public cSimpleModule
{
  protected:
    cMessage *msgServiced;
    cMessage *endServiceMsg;
    //one queue for each priority class
    cQueue * queue;
    int n;
    int queue_size;
    bool preemption;

    char name[10];
    char signalname[20];
    char statisticName[20];
    // line of code to get global variable ev
    cEnvir* ev = getEnvir();

    simsignal_t * qlenSignal;
    simsignal_t busySignal;
    simsignal_t * queueingTimeSignal;
    simsignal_t * responseTimeSignal;

    cProperty *statisticTemplateQlen;
    cProperty *statisticTemplateQueueingTime;
    cProperty *statisticTemplateResponseTime;

  public:
    Queue();
    virtual ~Queue();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};

Define_Module(Queue);


Queue::Queue()
{
    msgServiced = endServiceMsg = nullptr;
}

Queue::~Queue()
{
    delete msgServiced;
    cancelAndDelete(endServiceMsg);
}

void Queue::initialize()
{
    endServiceMsg = new cMessage("end-service");
    //get preemption parameter
    preemption = par("preemption");
    //n represent the number of classes
    n = par("numberofpriority");
    // queue for each priority class
    queue = new cQueue[n];
    // signal for each priority class
    qlenSignal = new simsignal_t[n];
    queueingTimeSignal = new simsignal_t[n];
    responseTimeSignal = new simsignal_t[n];
    // statistic template for each signal
    statisticTemplateQlen = getProperties()->get("statisticTemplate", "qlen");
    statisticTemplateQueueingTime = getProperties()->get("statisticTemplate", "queueingTime");
    statisticTemplateResponseTime = getProperties()->get("statisticTemplate", "responseTime");

    for ( int i=0; i<n ; i++ ){
        sprintf(name,"queue%d", i);
        // set the name of each queue
        queue[i].setName(name);
        // I register the signals
        // register qlen signal for each priority
        sprintf(signalname,"qlen%d", i);
        qlenSignal[i] = registerSignal(signalname);
        // register queueing time signal for each priority
        sprintf(signalname,"queueingTime%d", i);
        queueingTimeSignal[i] = registerSignal(signalname);
        // register restonse time signal for each priority
        sprintf(signalname,"responseTime%d", i);
        responseTimeSignal[i] = registerSignal(signalname);
        // I register the statistics
        // qlen statistic
        sprintf(statisticName, "qlen%d", i);
        ev->addResultRecorders(this, qlenSignal[i], statisticName, statisticTemplateQlen);
        // queueing time statistic
        sprintf(statisticName, "queueingTime%d", i);
        ev->addResultRecorders(this, queueingTimeSignal[i], statisticName, statisticTemplateQueueingTime);
        // response time statistic
        sprintf(statisticName, "responseTime%d", i);
        ev->addResultRecorders(this, responseTimeSignal[i], statisticName, statisticTemplateResponseTime);
    }

    //qlenSignal = registerSignal("qlen");
    busySignal = registerSignal("busy");
    //queueingTimeSignal = registerSignal("queueingTime");
    //responseTimeSignal = registerSignal("responseTime");

    emit(busySignal, false);
}

void Queue::handleMessage(cMessage *msg)
{
    if (msg == endServiceMsg) { // Self-message arrived

        EV << "Completed service of " << msgServiced->getName() << endl;
        send(msgServiced, "out");

        //Response time: time from msg arrival timestamp to time msg ends service (now)
        emit(responseTimeSignal[msgServiced->getSchedulingPriority()-1], simTime() - msgServiced->getTimestamp());
        //check all the classes if the relative queue is empty
        for(int i = 0; i < n; i++){
                if (queue[i].isEmpty()) { // Empty queue, server goes in IDLE
                    if(i==n-1){
                        EV << "Empty queue, server goes IDLE" <<endl;
                        msgServiced = nullptr;
                        emit(busySignal, false);
                    }
                }
                else
                {
                    //queue[i] is the higher non null priority queue
                    msgServiced = (cMessage *)queue[i].pop();
                    emit(qlenSignal[i], queue[i].getLength()); //Queue length changed, emit new length!

                    //Waiting time: time from msg arrival to time msg enters the server (now)
                    emit(queueingTimeSignal[msgServiced->getSchedulingPriority()-1], simTime() - msgServiced->getTimestamp());

                    EV << "Starting service of " << msgServiced->getName() << " with priority "<< msgServiced->getSchedulingPriority() << endl;
                    simtime_t serviceTime = par("serviceTime");
                    //scheduling a self message
                    scheduleAt(simTime()+serviceTime, endServiceMsg);
                    //force the exit to the loop
                    break;

                }

       }
    }
    else { // Data msg has arrived

        //Setting arrival timestamp as msg field
        msg->setTimestamp();
            if (!msgServiced) { //No message in service (server IDLE) ==> No queue ==> Direct service

                //ASSERT(queue.getLength() == 0);

                msgServiced = msg;
                //emit(queueingTimeSignal, SIMTIME_ZERO);

                EV << "Starting service of " << msgServiced->getName() <<" with priority " << msgServiced->getSchedulingPriority() << endl;
                simtime_t serviceTime = par("serviceTime");;
                scheduleAt(simTime()+serviceTime, endServiceMsg);
                emit(busySignal, true);
            }
            else {  //Message in service (server BUSY) ==> Queuing
                if(preemption){
                            if(msg->getSchedulingPriority() < msgServiced->getSchedulingPriority()){
                                //put out low priority message
                                EV << msgServiced->getName()<< " getting out from service from a message with high priority" << endl;
                                // I put the low priority message at top of it's queue
                                queue[msgServiced->getSchedulingPriority()-1].insert(msgServiced);
                                EV << msgServiced->getName()<< " enter in the queue " << queue[msgServiced->getSchedulingPriority()-1].getName() << endl;
                                // emit the signal with the new queue length
                                emit(qlenSignal[msgServiced->getSchedulingPriority()-1],queue[msgServiced->getSchedulingPriority()-1].getLength());
                                // I delete the send of a self-message of low priority messages
                                cancelEvent(endServiceMsg);
                                //put in service new high priority service
                                msgServiced = msg;
                                EV << "Preemption: Starting service of " << msgServiced->getName() <<" with priority " << msg->getSchedulingPriority() << endl;
                                simtime_t serviceTime = par("serviceTime");;
                                scheduleAt(simTime()+serviceTime, endServiceMsg);
                                return;
                            }
                }
                // if there is not set the preemption or the message has lower priority
                queue_size=par("queue_size");
                // I check all priorities
                for(int i=0; i<n; i++){
                    // I need to subtract 1 to priorities propriety
                    // because my queue starts from 0 while priorities starts from 1
                    if(msg->getSchedulingPriority()-1==i){
                        if(queue[i].getLength()<queue_size){
                            EV << msg->getName() << " enters queue"<< endl;
                            queue[i].insert(msg);
                            emit(qlenSignal[i], queue[i].getLength()); //Queue length changed, emit new length!
                        }
                        else{
                            EV << msg->getName() << " with priority " << msg->getSchedulingPriority() << " is dropped from the system due to queue full"<< endl;
                        }
                    }
                }
           }
        }

   }
