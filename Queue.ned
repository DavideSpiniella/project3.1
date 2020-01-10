//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//
simple Queue
{
    parameters:
        volatile double serviceTime @unit(s);
        volatile int queue_size;
        volatile bool preemption;
        volatile int numberofpriority; 
        @display("i=block/queue;q=queue");
        
        @signal[qlen*](type="long");
        @signal[busy](type="bool");
        @signal[queueingTime*](type="simtime_t");
        @signal[responseTime*](type="simtime_t");
        
        @statisticTemplate[qlen](title="queue length";record=vector,timeavg,max;interpolationmode=sample-hold);
        @statistic[busy](title="server busy state";record=vector?,timeavg;interpolationmode=sample-hold);
        @statisticTemplate[queueingTime](title="queueing time at dequeue";unit=s;record=vector,mean,max;interpolationmode=none);
        @statisticTemplate[responseTime](title="response time at end of service";unit=s;record=vector,mean,max;interpolationmode=none);
    
    gates:
        input in;
        output out;
}