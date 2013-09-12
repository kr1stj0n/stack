package rina.utils.apps.rinaband.server;

import java.util.Hashtable;
import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;

import eu.irati.librina.ApplicationProcessNamingInformation;
import eu.irati.librina.ApplicationRegistration;
import eu.irati.librina.ApplicationRegistrationInformation;
import eu.irati.librina.ApplicationRegistrationType;
import eu.irati.librina.Flow;
import eu.irati.librina.FlowDeallocatedEvent;
import eu.irati.librina.FlowRequestEvent;
import eu.irati.librina.IPCManagerSingleton;
import eu.irati.librina.rina;

import rina.cdap.api.CDAPSessionManager;
import rina.cdap.api.message.CDAPMessage;
import rina.cdap.api.message.ObjectValue;
import rina.utils.apps.rinaband.StatisticsInformation;
import rina.utils.apps.rinaband.TestInformation;
import rina.utils.apps.rinaband.protobuf.RINABandStatisticsMessageEncoder;
import rina.utils.apps.rinaband.protobuf.RINABandTestMessageEncoder;
import rina.utils.apps.rinaband.utils.FlowAcceptor;
import rina.utils.apps.rinaband.utils.FlowDeallocationListener;
import rina.utils.apps.rinaband.utils.FlowReader;
import rina.utils.apps.rinaband.utils.IPCEventConsumer;
import rina.utils.apps.rinaband.utils.SDUListener;

/**
 * Controls the negotiation of test parameters and 
 * the execution of a single test
 * @author eduardgrasa
 *
 */
public class TestController implements SDUListener, FlowAcceptor, FlowDeallocationListener{
	
	private enum State {WAIT_CREATE, WAIT_START, EXECUTING, WAIT_STOP, COMPLETED};
	
	/**
	 * The state of the test
	 */
	private State state = State.WAIT_CREATE;
	
	/**
	 * The information of this test
	 */
	private TestInformation testInformation = null;
	
	/**
	 * The APNamingInfo associated to the data AE of the RINABand application
	 */
	private ApplicationProcessNamingInformation dataApNamingInfo = null;
	
	/**
	 * The CDAPSessionManager
	 */
	private CDAPSessionManager cdapSessionManager = null;
	
	/**
	 * The flow from the RINABand client
	 */
	private Flow flow = null;
	
	/**
	 * The map of allocated flows, with the 
	 * classes that deal with each individual flow
	 */
	private Map<Integer, TestWorker> allocatedFlows = null;
	
	/**
	 * The stop test message
	 */
	private CDAPMessage stopTestMessage = null;
	
	private IPCEventConsumer ipcEventConsumer = null;
	
	private ApplicationRegistration dataAERegistration = null;
	
	private ApplicationProcessNamingInformation difName = null;
	
	/**
	 * Epoch times are in miliseconds
	 */
	private long epochTimeFirstSDUReceived = 0;
	private long epochTimeLastSDUReceived = 0;
	private int completedReceives = 0;
	
	private long epochTimeFirstSDUSent = 0;
	private long epochTimeLastSDUSent = 0;
	private int completedSends = 0;
	
	public TestController(ApplicationProcessNamingInformation dataApNamingInfo, 
			ApplicationProcessNamingInformation difName, Flow flow,
			CDAPSessionManager cdapSessionManager, IPCEventConsumer ipcEventConsumer){
		this.dataApNamingInfo = dataApNamingInfo;
		System.out.println("DIF name is "+difName);
		this.difName = difName;
		this.cdapSessionManager = cdapSessionManager;
		this.flow = flow;
		this.allocatedFlows = new Hashtable<Integer, TestWorker>();
		this.ipcEventConsumer = ipcEventConsumer;
	}

	public void sduDelivered(byte[] sdu) {
		try{
			CDAPMessage cdapMessage = this.cdapSessionManager.decodeCDAPMessage(sdu);
			System.out.println("Received CDAP Message: "+cdapMessage.toString());
			switch(cdapMessage.getOpCode()){
			case M_CREATE:
				handleCreateMessageReceived(cdapMessage);
				break;
			case M_START:
				handleStartMessageReceived(cdapMessage);
				break;
			case M_STOP:
				handleStopMessageReceived(cdapMessage);
				break;
			default:
				printMessage("Received CDAP Message with wrong opcode, ignoring it.");
			}
		}catch(Exception ex){
			printMessage("Error decoding CDAP Message.");
			ex.printStackTrace();
		}
	}
	
	/**
	 * Check the data in the TestInformation object, change the values 
	 * that we do not agree with and register the Data AE that will 
	 * receive the test flow Allocations
	 * @param cdapMessage
	 */
	private void handleCreateMessageReceived(CDAPMessage cdapMessage){
		if (this.state != State.WAIT_CREATE){
			printMessage("Received CREATE Test message while not in WAIT_CREATE state." + 
			" Ignoring it.");
			return;
		}
		
		ObjectValue objectValue = cdapMessage.getObjValue();
		if (objectValue == null || objectValue.getByteval() == null){
			printMessage("The create message did not contain an object value. Ignoring the message");
			return;
		}
		
		try{
			//1 Decode and update the testInformation object
			this.testInformation = RINABandTestMessageEncoder.decode(objectValue.getByteval());
			this.testInformation.setAei(""+flow.getPortId());
			if (this.testInformation.getNumberOfFlows() > RINABandServer.MAX_NUMBER_OF_FLOWS){
				this.testInformation.setNumberOfFlows(RINABandServer.MAX_NUMBER_OF_FLOWS);
			}
			if (this.testInformation.getNumberOfSDUs() > RINABandServer.MAX_SDUS_PER_FLOW){
				this.testInformation.setNumberOfSDUs(RINABandServer.MAX_SDUS_PER_FLOW);
			}
			if (this.testInformation.getSduSize() > RINABandServer.MAX_SDU_SIZE_IN_BYTES){
				this.testInformation.setSduSize(RINABandServer.MAX_SDU_SIZE_IN_BYTES);
			}
			
			//2 Update the DATA AE and register it
			this.dataApNamingInfo.setEntityInstance(this.testInformation.getAei());
			ApplicationRegistrationInformation appRegInfo = new ApplicationRegistrationInformation();
			//TODO fix this and replace it for the = new ApplicationRegistrationInformation();
					/*new ApplicationRegistrationInformation(ApplicationRegistrationType.APPLICATION_REGISTRATION_SINGLE_DIF);
			appRegInfo.setDIFName(difName);*/
			dataAERegistration = rina.getIpcManager().registerApplication(this.dataApNamingInfo, appRegInfo);
			ipcEventConsumer.addFlowAcceptor(this, dataApNamingInfo);
			
			//3 Reply and update state
			CDAPMessage replyMessage = cdapMessage.getReplyMessage();
			objectValue.setByteval(RINABandTestMessageEncoder.encode(this.testInformation));
			replyMessage.setObjValue(objectValue);
			sendCDAPMessage(replyMessage);
			this.state = State.WAIT_START;
			printMessage("Waiting to START a new test with the following parameters.");
			printMessage(this.testInformation.toString());
		}catch(Exception ex){
			printMessage("Error handling CREATE Test message.");
			ex.printStackTrace();
		}
		
	}
	
	/**
	 * Start the test. Be prepared to accept new flows, receive data and/or to create 
	 * new flows and write data
	 * @param cdapMessage
	 */
	private void handleStartMessageReceived(CDAPMessage cdapMessage){
		if (this.state != State.WAIT_START){
			printMessage("Received START Test message while not in WAIT_START state." + 
			" Ignoring it.");
			return;
		}
		
		Iterator<Entry<Integer, TestWorker>> iterator = allocatedFlows.entrySet().iterator();
		while(iterator.hasNext()){
			iterator.next().getValue().execute();
		}
		
		this.state = State.EXECUTING;
		printMessage("Started test execution");
	}
	
	private synchronized void handleStopMessageReceived(CDAPMessage cdapMessage){
		if (this.state == State.EXECUTING){
			this.stopTestMessage = cdapMessage;
			printMessage("Received STOP Test message while still in EXECUTING state." + 
			" Storing it and waiting to send/receive all test data.");
			return;
		}else if (this.state != State.WAIT_STOP){
			printMessage("Received STOP Test message while not in EXECUTING or WAIT_STOP state, ignoring it");
			return;
		}
		
		this.stopTestMessage = cdapMessage;
		this.printStatsAndFinishTest();
	}
	
	public synchronized void setFirstSDUSent(long epochTime){
		if (this.epochTimeFirstSDUSent == 0){
			this.epochTimeFirstSDUSent = epochTime;
		}
	}
	
	public synchronized void setFirstSDUReveived(long epochTime){
		if (this.epochTimeFirstSDUReceived == 0){
			this.epochTimeFirstSDUReceived = epochTime;
		}
	}
	
	public synchronized void setLastSDUSent(long epochTime){
		this.completedSends++;
		if (this.completedSends == this.testInformation.getNumberOfFlows()){
			this.epochTimeLastSDUSent = epochTime;
			if (!this.testInformation.isClientSendsSDUs() || this.completedReceives == this.testInformation.getNumberOfFlows()){
				this.changeToWaitStopState();
			}
		}
	}
	
	public synchronized void setLastSDUReceived(long epochTime){
		this.completedReceives++;
		if (this.completedReceives == this.testInformation.getNumberOfFlows()){
			this.epochTimeLastSDUReceived = epochTime;
			if (!this.testInformation.isServerSendsSDUs() || this.completedSends == this.testInformation.getNumberOfFlows()){
				this.changeToWaitStopState();
			}
		}
	}
	
	private void changeToWaitStopState(){
		this.state = State.WAIT_STOP;
		if (this.stopTestMessage != null){
			printStatsAndFinishTest();
		}
	}
	
	private void printStatsAndFinishTest(){
		//1 Write statistics as response and print the stats
		try{
			//Update the statistics and send the M_STOP_R message
			StatisticsInformation statsInformation = RINABandStatisticsMessageEncoder.decode(this.stopTestMessage.getObjValue().getByteval());
			if (this.testInformation.isClientSendsSDUs()){
				statsInformation.setServerTimeFirstSDUReceived(this.epochTimeFirstSDUReceived*1000L);
				statsInformation.setServerTimeLastSDUReceived(this.epochTimeLastSDUReceived*1000L);
			}
			if (this.testInformation.isServerSendsSDUs()){
				statsInformation.setServerTimeFirstSDUSent(this.epochTimeFirstSDUSent*1000L);
				statsInformation.setServerTimeLastSDUSent(this.epochTimeLastSDUSent*1000L);
			}
			printMessage(statsInformation.toString());
			ObjectValue objectValue = new ObjectValue();
			objectValue.setByteval(RINABandStatisticsMessageEncoder.encode(statsInformation));
			CDAPMessage responseMessage = CDAPMessage.getStopObjectResponseMessage(null, 0, null, this.stopTestMessage.getInvokeID());
			responseMessage.setObjClass(this.stopTestMessage.getObjClass());
			responseMessage.setObjName(this.stopTestMessage.getObjName());
			responseMessage.setObjValue(objectValue);
			sendCDAPMessage(responseMessage);
			
			//Print aggregate statistics
			long averageClientServerDelay = 0L;
			long averageServerClientDelay = 0L;
			printMessage("Aggregate bandwidth:");
			if (this.testInformation.isClientSendsSDUs()){
				long aggregateReceivedSDUsPerSecond = 1000L*this.testInformation.getNumberOfFlows()*
					this.testInformation.getNumberOfSDUs()/(this.epochTimeLastSDUReceived-this.epochTimeFirstSDUReceived);
				printMessage("Aggregate received SDUs per second: "+aggregateReceivedSDUsPerSecond);
				averageClientServerDelay = ((this.epochTimeFirstSDUReceived - statsInformation.getClientTimeFirstSDUSent()/1000L) + 
						(this.epochTimeLastSDUReceived - statsInformation.getClientTimeLastSDUSent()/1000L))/2;
				printMessage("Aggregate received KiloBytes per second (KBps): "+ aggregateReceivedSDUsPerSecond*this.testInformation.getSduSize()/1024);
				printMessage("Aggregate received Megabits per second (Mbps): "+ aggregateReceivedSDUsPerSecond*this.testInformation.getSduSize()*8/(1024*1024));
			}
			if (this.testInformation.isServerSendsSDUs()){
				long aggregateSentSDUsPerSecond = 1000L*1000L*this.testInformation.getNumberOfFlows()*
					this.testInformation.getNumberOfSDUs()/(statsInformation.getClientTimeLastSDUReceived()-statsInformation.getClientTimeFirstSDUReceived());
				averageServerClientDelay = ((statsInformation.getClientTimeFirstSDUReceived()/1000L - this.epochTimeFirstSDUSent) + 
						(statsInformation.getClientTimeLastSDUReceived()/1000L - this.epochTimeLastSDUSent))/2;
				printMessage("Aggregate sent SDUs per second: "+aggregateSentSDUsPerSecond);
				printMessage("Aggregate sent KiloBytes per second (KBps): "+ aggregateSentSDUsPerSecond*this.testInformation.getSduSize()/1024);
				printMessage("Aggregate sent Megabits per second (Mbps): "+ aggregateSentSDUsPerSecond*this.testInformation.getSduSize()*8/(1024*1024));
			}
			long rttInMs = 0L;
			if (this.testInformation.isClientSendsSDUs() && this.testInformation.isServerSendsSDUs()){
				rttInMs = averageClientServerDelay + averageServerClientDelay;
			}else if (this.testInformation.isClientSendsSDUs()){
				rttInMs = averageClientServerDelay*2;
			}else{
				rttInMs = averageServerClientDelay*2;
			}
			printMessage("Estimated round-trip time (RTT) in ms: "+rttInMs);
		}catch(Exception ex){
			printMessage("Problems returning STOP RESPONSE message");
			ex.printStackTrace();
		}
		
		//2 Cancel the registration of the data AE
		this.state = State.COMPLETED;
		try{
			rina.getIpcManager().unregisterApplication(dataApNamingInfo, dataAERegistration.getDIFNames().getFirst());
			ipcEventConsumer.removeFlowAcceptor(dataApNamingInfo);
		}catch(Exception ex){
			printMessage("Problems unregistering data AE");
			ex.printStackTrace();
		}
	}

	/**
	 * Called when a new data flow has been allocated
	 */
	public synchronized void flowAllocated(Flow flow) {
		if (this.state != State.WAIT_START){
			printMessage("New flow allocated, but we're not in the WAIT_START state. Requesting deallocation.");
			try{
				rina.getIpcManager().deallocateFlow(flow.getPortId());
			}catch(Exception ex){
				ex.printStackTrace();
			}
			return;
		}
		
		TestWorker testWorker = new TestWorker(this.testInformation, flow, this);
		FlowReader flowReader = new FlowReader(flow, testWorker, this.testInformation.getSduSize());
		RINABandServer.executeRunnable(flowReader);
		this.allocatedFlows.put(new Integer(flow.getPortId()), testWorker);
		ipcEventConsumer.addFlowDeallocationListener(this, flow.getPortId());
		printMessage("Data flow with portId "+flow.getPortId()+ " allocated");
	}

	/**
	 * Called when an existing data flow has been deallocated
	 */
	public synchronized void flowDeallocated(int portId) {
		if (this.state == State.COMPLETED){
			this.allocatedFlows.remove(new Integer(portId));
			printMessage("Data flow with portId "+portId+ " deallocated");
		}
		
		ipcEventConsumer.removeFlowDeallocationListener(portId);
	}
	
	/**
	 * Decide when a flow can be accepted
	 */
	public void dispatchFlowRequestEvent(FlowRequestEvent event){
		IPCManagerSingleton ipcManager = rina.getIpcManager();
		
		try{
			Flow flow = ipcManager.allocateFlowResponse(event, true, "ok");
			flowAllocated(flow);
		}catch(Exception ex){
			ex.printStackTrace();
		}
	}
	
	public void dispatchFlowDeallocatedEvent(FlowDeallocatedEvent event){
		flowDeallocated(event.getPortId());
	}
	
	private void printMessage(String message){
		System.out.println("Test controller "+flow.getPortId()+": " + message);
	}
	
	private void sendCDAPMessage(CDAPMessage cdapMessage) throws Exception{
		byte[] sdu = cdapSessionManager.encodeCDAPMessage(cdapMessage);
		flow.writeSDU(sdu, sdu.length);
	}
	
}
