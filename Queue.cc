#include <omnetpp.h>

using namespace omnetpp;


class Queue : public cSimpleModule
{
  protected:
    cMessage *msgServiced;
    cMessage *endServiceMsg;
    cMessage *endSlotClassMsg;
    //one queue for each priority class
    cQueue * queue;
    int n;
    int index;

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
    endSlotClassMsg= new cMessage("end-slot");
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
        //ho finito il servizio prima della fine dello slot quindi cancello l'evento fine
        //dello slot
        cancelEvent(endSlotClassMsg);
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
                if (queue[index-1].isEmpty()) { // Empty queue, server goes in IDLE
                        EV << "Empty queue, server goes IDLE" <<endl;
                        msgServiced = nullptr;
                        emit(busySignal, false);
                }
                else
                {
                    EV << "now we are serving users of class "<< index << endl;
                    //queue[i] is the higher non null priority queue
                    msgServiced = (cMessage *)queue[index-1].pop();
                    emit(qlenSignal[index-1], queue[index-1].getLength()); //Queue length changed, emit new length!

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
                    scheduleAt(simTime()+slotTime, endSlotClassMsg);


                }
    }
    else if(msg==endSlotClassMsg){

        EV << "finish of slot " << msgServiced->getSchedulingPriority() << endl;
                //togliamo il messaggio dal servizio, lo rimettiamo nella sua coda e mettiamo
                //in servizio il messaggio della classe successiva
                cancelEvent(endServiceMsg);
                queue[index-1].insert(msgServiced);
                //controllo se sono all'ultima classe se sono all'ultima riparto da 1
                //altrimenti vado alla successiva
                if(index==n){
                    index=1;
                }else{
                    index++;
                }
                //TODO: in questo caso spreco uno slot da vedere se implementarla diversamente
                        if (queue[index-1].isEmpty()) { // Empty queue, server goes in IDLE
                                EV << "Empty queue, server goes IDLE" <<endl;
                                msgServiced = nullptr;
                                emit(busySignal, false);
                        }
                        else
                        {
                            EV << "now we are serving users of class "<< index << endl;
                            //queue[i] is the higher non null priority queue
                            msgServiced = (cMessage *)queue[index-1].pop();
                            emit(qlenSignal[index-1], queue[index-1].getLength()); //Queue length changed, emit new length!

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
                            scheduleAt(simTime()+slotTime, endSlotClassMsg);


                        }

    }
    else { // Data msg has arrived

        //Setting arrival timestamp as msg field
        msg->setTimestamp();
            if (!msgServiced) { //No message in service (server IDLE) ==> No queue ==> Direct service

                //ASSERT(queue.getLength() == 0);

                msgServiced = msg;
                //emit(queueingTimeSignal, SIMTIME_ZERO);

                EV << "Starting service of " << msgServiced->getName() <<" with class " << msgServiced->getSchedulingPriority() << endl;
                //setto il parametro corretto
                sprintf(param, "serviceTime%d", msgServiced->getSchedulingPriority());
                simtime_t serviceTime = par(param);;
                scheduleAt(simTime()+serviceTime, endServiceMsg);
                emit(busySignal, true);
            }
            else {  //Message in service (server BUSY) ==> Queuing
                for(int i=0; i<n; i++){
                    // I need to subtract 1 to priorities propriety
                    // because my queue starts from 0 while priorities starts from 1
                    if(msg->getSchedulingPriority()-1==i){
                            EV << msg->getName() << " enters queue"<< endl;
                            queue[i].insert(msg);
                            emit(qlenSignal[i], queue[i].getLength()); //Queue length changed, emit new length!
                            break;
                    }
                }
           }
        }

   }
