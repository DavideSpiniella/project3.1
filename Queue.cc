#include <omnetpp.h>
#include <stdlib.h>

using namespace omnetpp;


class Queue : public cSimpleModule
{
  protected:
    cMessage *msgServiced;
    cMessage *endServiceMsg;
    cMessage *endSlotClassMsg;
    //  variabili service time e slot time
    simtime_t serviceTime;
    simtime_t slotTime;
    //one queue for each scheduling class
    cQueue * queue;
    //number of classes
    int numberofSchedulingClasses;
    // index per scorrere le classi dello scheduler
    int index;
    int * serviceTimes;
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
    // inizializzo l'indice delle classi dello scheduler
    // (per convenzione parte da 1)
    index=1;
    endServiceMsg = new cMessage("end-service");
    endSlotClassMsg= new cMessage("end-slot");
    // numberofSchedulingClasses represent the number of scheduling classes
    numberofSchedulingClasses = par("numberofSchedulingClasses");
    // queue for each scheduling classes
    queue = new cQueue[numberofSchedulingClasses];
    // così dichiaro l'array con una posizione per ogni classe
    serviceTimes = (int*) malloc(sizeof(int) * numberofSchedulingClasses);
    // inizializzo la variabile slot time
    slotTime=par("timeSlot");
    // scheduling a self message of end of the first slot
    scheduleAt(simTime()+slotTime, endSlotClassMsg);
    // signal for each scheduler class
    qlenSignal = new simsignal_t[numberofSchedulingClasses];
    queueingTimeSignal = new simsignal_t[numberofSchedulingClasses];
    responseTimeSignal = new simsignal_t[numberofSchedulingClasses];
    // statistic template for each signal
    statisticTemplateQlen = getProperties()->get("statisticTemplate", "qlen");
    statisticTemplateQueueingTime = getProperties()->get("statisticTemplate", "queueingTime");
    statisticTemplateResponseTime = getProperties()->get("statisticTemplate", "responseTime");

    for ( int i=0; i < numberofSchedulingClasses; i++ ){
        sprintf(name,"queue%d", i);
        // set the name of each queue
        queue[i].setName(name);
        // per ogni classe scelgo un service time casualmente tra quelli disponibili
        serviceTimes[i]=rand()%5+1;
        // I register the signals
        // register qlen signal for each class
        sprintf(signalname,"qlen%d", i);
        qlenSignal[i] = registerSignal(signalname);
        // register queueing time signal for each class
        sprintf(signalname,"queueingTime%d", i);
        queueingTimeSignal[i] = registerSignal(signalname);
        // register restonse time signal for each class
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

    busySignal = registerSignal("busy");


    emit(busySignal, false);
}

void Queue::handleMessage(cMessage *msg)
{
    if (msg == endServiceMsg) { // Self-message arrived

        EV << "Completed service of " << msgServiced->getName() << endl;
        send(msgServiced, "out");
        //Response time: time from msg arrival timestamp to time msg ends service (now)
        emit(responseTimeSignal[msgServiced->getSchedulingPriority()-1], simTime() - msgServiced->getTimestamp());
        // in questo caso uso ancora msgServiced perchè sono della stessa classe
                if (queue[msgServiced->getSchedulingPriority()-1].isEmpty()) { // Empty queue of this class server goes in IDLE
                        EV << "Empty queue, server goes IDLE" <<endl;
                        msgServiced = nullptr;
                        emit(busySignal, false);
                }
                else
                {
                    EV << "now we are serving users of class "<< index << endl;
                    // prendo l' utente che devo servire dalla sua coda
                    // in questo caso uso ancora msgServiced perchè sono della stessa classe
                    msgServiced = (cMessage *)queue[msgServiced->getSchedulingPriority()-1].pop();
                    emit(qlenSignal[msgServiced->getSchedulingPriority()-1], queue[msgServiced->getSchedulingPriority()-1].getLength()); //Queue length changed, emit new length!

                    //Waiting time: time from msg arrival to time msg enters the server (now)
                    emit(queueingTimeSignal[msgServiced->getSchedulingPriority()-1], simTime() - msgServiced->getTimestamp());

                    EV << "Starting service of " << msgServiced->getName() << " of class "<< msgServiced->getSchedulingPriority() << endl;
                    // scelgo il service time corrispondente alla mia classe
                    sprintf(param, "serviceTime%d", serviceTimes[msgServiced->getSchedulingPriority()-1]);
                    serviceTime = par(param);
                    //scheduling a self message of end of service
                    scheduleAt(simTime()+serviceTime, endServiceMsg);


                }
    }
    else if(msg==endSlotClassMsg){

        EV << "finish of slot " << index << endl;
                // togliamo il messaggio dal servizio, lo rimettiamo nella sua coda e mettiamo
                // in servizio il messaggio della classe successiva
                if(msgServiced){
                    cancelEvent(endServiceMsg);
                    queue[index-1].insert(msgServiced);
                }
                // controllo se sono all'ultima classe se sono all'ultima riparto da 1
                // altrimenti vado alla successiva
                if(index == numberofSchedulingClasses){
                    index=1;
                }else{
                    index++;
                }
                if (queue[index-1].isEmpty()) { // Empty queue of this class, server goes in IDLE
                        EV << "Empty queue, server goes IDLE" <<endl;
                        msgServiced = nullptr;
                        emit(busySignal, false);
                }
                else
                {
                    EV << "now we are serving users of class "<< index << endl;
                    // prendo l' utente che devo servire dalla sua coda
                    msgServiced = (cMessage *)queue[index-1].pop();
                    emit(qlenSignal[index-1], queue[index-1].getLength()); //Queue length changed, emit new length!

                    // Waiting time: time from msg arrival to time msg enters the server (now)
                    emit(queueingTimeSignal[msgServiced->getSchedulingPriority()-1], simTime() - msgServiced->getTimestamp());

                    EV << "Starting service of " << msgServiced->getName() << " of class "<< msgServiced->getSchedulingPriority() << endl;
                    // scelgo il service time corrispondente alla mia classe
                    sprintf(param, "serviceTime%d", serviceTimes[msgServiced->getSchedulingPriority()-1]);
                    serviceTime = par(param);
                    // scheduling a self message of end of service
                    scheduleAt(simTime()+serviceTime, endServiceMsg);
                }
                // scheduling a self message of end of slot
                scheduleAt(simTime()+slotTime, endSlotClassMsg);

    }
    else { // Data msg has arrived

        // Setting arrival timestamp as msg field
        msg->setTimestamp();
            if (!msgServiced) {
                if(msg->getSchedulingPriority() == index){
                    msgServiced = msg;
                    EV << "Starting service of " << msgServiced->getName() <<" with class " << msgServiced->getSchedulingPriority() << endl;
                    // scelgo il service time corrispondente alla mia classe
                    sprintf(param, "serviceTime%d", serviceTimes[msgServiced->getSchedulingPriority()-1]);
                    serviceTime = par(param);;
                    scheduleAt(simTime()+serviceTime, endServiceMsg);
                    emit(busySignal, true);
                }
                else{
                    EV << "message " << msg->getName() << " enter in the queue because is not it's turn" <<endl;
                    // inserisco il messaggio che mi è arrivato nella sua coda
                    queue[msg->getSchedulingPriority()-1].insert(msg);
                    // Queue length changed, emit new length!
                    emit(qlenSignal[msg->getSchedulingPriority()-1], queue[msg->getSchedulingPriority()-1].getLength());
                    EV << "Empty queue, server goes IDLE" <<endl;
                    emit(busySignal, false);
                }
            }
            else {  // Message in service (server BUSY) ==> Queuing
                for(int i=0; i<numberofSchedulingClasses; i++){
                    // I need to subtract 1 to getSchedulingPriority()
                    // because my queue starts from 0 while classes starts from 1
                    if(msg->getSchedulingPriority()-1==i){
                            EV << msg->getName() << " enters queue"<< endl;
                            queue[i].insert(msg);
                            // Queue length changed, emit new length!
                            emit(qlenSignal[i], queue[i].getLength());
                            break;
                    }
                }
           }
        }

   }
