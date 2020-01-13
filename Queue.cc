#include <omnetpp.h>

using namespace omnetpp;


class Queue : public cSimpleModule
{
  protected:
    cMessage *msgServiced;
    cMessage *endServiceMsg;
    cMessage *endSlotClass;
    //one queue for each priority class
    cQueue * queue;
    int n;
    int index;
    int queue_size;
    bool preemption;

    char name[10];
    char param[20];
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
    //inizializzo l'indice delle priorità
    index=1;
    endServiceMsg = new cMessage("end-service");
    endSlotClass= new cMessage("end-slot");
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
        //TODO: vedere come fargli stamapare la classe di priorità
        EV << "now we are serving users of class ";
        //Response time: time from msg arrival timestamp to time msg ends service (now)
        emit(responseTimeSignal[msgServiced->getSchedulingPriority()-1], simTime() - msgServiced->getTimestamp());
        //controllo se sono all'ultima classe se sono all'ultima riparto da 1
        //altrimenti vado alla successiva
        if(index==n){
            index=1;
        }else{
            index++;
        }
        //TODO: in questo caso spreco uno slot da vedere se implementarla diversamente
                if (queue[index].isEmpty()) { // Empty queue, server goes in IDLE
                        EV << "Empty queue, server goes IDLE" <<endl;
                        msgServiced = nullptr;
                        emit(busySignal, false);
                }
                else
                {
                    //queue[i] is the higher non null priority queue
                    msgServiced = (cMessage *)queue[index].pop();
                    emit(qlenSignal[index], queue[index].getLength()); //Queue length changed, emit new length!

                    //Waiting time: time from msg arrival to time msg enters the server (now)
                    emit(queueingTimeSignal[msgServiced->getSchedulingPriority()-1], simTime() - msgServiced->getTimestamp());

                    EV << "Starting service of " << msgServiced->getName() << " of class "<< msgServiced->getSchedulingPriority() << endl;
                    //setto il parametro corretto
                    sprintf(param, "serviceTime%d", msgServiced->getSchedulingPriority());
                    simtime_t serviceTime = par(param);
                    //scheduling a self message of end of service
                    scheduleAt(simTime()+serviceTime, endServiceMsg);
                    simtime_t slotTime=par("timeSlot");
                    //scheduling a self message of end of slot


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
                //setto il parametro corretto
                sprintf(param, "serviceTime%d", msgServiced->getSchedulingPriority());
                simtime_t serviceTime = par(param);;
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
                                //setto il parametro corretto
                                sprintf(param, "serviceTime%d", msgServiced->getSchedulingPriority());
                                simtime_t serviceTime = par(param);;
                                scheduleAt(simTime()+serviceTime, endServiceMsg);
                                return;
                            }
                }
                // if there is not set the preemption or the message has lower priority
                // I check all priorities
                for(int i=0; i<n; i++){
                    // I need to subtract 1 to priorities propriety
                    // because my queue starts from 0 while priorities starts from 1
                    if(msg->getSchedulingPriority()-1==i){
                            EV << msg->getName() << " enters queue"<< endl;
                            queue[i].insert(msg);
                            emit(qlenSignal[i], queue[i].getLength()); //Queue length changed, emit new length!
                    }
                }
           }
        }

   }
