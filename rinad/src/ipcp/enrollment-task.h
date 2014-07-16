/*
 * Enrollment Task
 *
 *    Bernat Gaston <bernat.gaston@i2cat.net>
 *    Eduard Grasa <eduard.grasa@i2cat.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef IPCP_ENROLLMENT_TASK_HH
#define IPCP_ENROLLMENT_TASK_HH

#ifdef __cplusplus

#include <map>

#include "ipcp/components.h"
#include <librina/cdap.h>
#include <librina/timer.h>

namespace rinad {

/// The object that contains all the information
/// that is required to initiate an enrollment
/// request (send as the objectvalue of a CDAP M_START
/// message, as specified by the Enrollment spec)
class EnrollmentInformationRequest {
public:
	EnrollmentInformationRequest();
	unsigned int get_address() const;
	void set_address(unsigned int address);
	const std::list<rina::ApplicationProcessNamingInformation>& get_supporting_difs() const;
	void set_supporting_difs(const std::list<rina::ApplicationProcessNamingInformation> &supporting_difs);

private:
	/// The address of the IPC Process that requests
	///to join a DIF
	unsigned int address_;
	std::list<rina::ApplicationProcessNamingInformation> supporting_difs_;
};

class WatchdogRIBObject;

/// Sends a CDAP message to check that the CDAP connection
/// is still working ok
class WatchdogTimerTask: public rina::TimerTask {
public:
	WatchdogTimerTask(WatchdogRIBObject * watchdog, rina::Timer * timer, int delay);
	void run();

private:
	WatchdogRIBObject * watchdog_;
	rina::Timer * timer_;
	int delay_;
};

class WatchdogRIBObject: public BaseRIBObject, public BaseCDAPResponseMessageHandler {
public:
	WatchdogRIBObject(IPCProcess * ipc_process, const rina::DIFConfiguration& dif_configuration);
	~WatchdogRIBObject();
	const void* get_value() const;
	void remoteReadObject(const rina::CDAPMessage * cdapMessage,
				rina::CDAPSessionDescriptor * cdapSessionDescriptor);

	/// Send watchdog messages to the IPC processes that are our neighbors and we're enrolled to
	void sendMessages();

	/// Take advantadge of the watchdog message responses to measure the RTT,
	/// and store it in the neighbor object (average of the last 4 RTTs)
	void readResponse(const rina::CDAPMessage * cdapMessage,
				rina::CDAPSessionDescriptor * cdapSessionDescriptor);

private:
	rina::CDAPSessionManagerInterface * cdap_session_manager_;
	rina::Timer * timer_;
	int wathchdog_period_;
	int declared_dead_interval_;
	std::map<std::string, int> neighbor_statistics_;
	rina::Lockable * lock_;
};

class NeighborSetRIBObject: public BaseRIBObject {
public:
	NeighborSetRIBObject(IPCProcess * ipc_process);
	~NeighborSetRIBObject();
	const void* get_value() const;
	void remoteCreateObject(const rina::CDAPMessage * cdapMessage,
			rina::CDAPSessionDescriptor * cdapSessionDescriptor);
	void createObject(const std::string& objectClass,
			const std::string& objectName,
			const void* objectValue);

private:
	void populateNeighborsToCreateList(rina::Neighbor * neighbor,
			std::list<rina::Neighbor *> * list);
	void createNeighbor(rina::Neighbor * neighbor);

	rina::Lockable * lock_;
};

/// Handles the operations related to the "daf.management.naming.currentsynonym" objects
class AddressRIBObject: public BaseRIBObject {
public:
	AddressRIBObject(IPCProcess * ipc_process);
	~AddressRIBObject();
	const void* get_value() const;
	void writeObject(const void* object_value);

private:
	unsigned int address_;
};

class BaseEnrollmentStateMachine;

class EnrollmentFailedTimerTask: public rina::TimerTask {
public:
	EnrollmentFailedTimerTask(BaseEnrollmentStateMachine * state_machine,
			const std::string& reason, bool enrollee);
	~EnrollmentFailedTimerTask() throw() {};
	void run();

	BaseEnrollmentStateMachine * state_machine_;
	std::string reason_;
	bool enrollee_;
};

/// The base class that contains the common aspects of both
/// enrollment state machines: the enroller side and the enrolle
/// side
class BaseEnrollmentStateMachine : public BaseCDAPResponseMessageHandler {
	friend class EnrollmentFailedTimerTask;
public:
	static const std::string CONNECT_RESPONSE_TIMEOUT;
	static const std::string START_RESPONSE_TIMEOUT;
	static const std::string START_IN_BAD_STATE;
	static const std::string STOP_ENROLLMENT_TIMEOUT;
	static const std::string STOP_IN_BAD_STATE;
	static const std::string STOP_WITH_NO_OBJECT_VALUE;
	static const std::string READ_RESPONSE_TIMEOUT;
	static const std::string PROBLEMS_COMMITTING_ENROLLMENT_INFO;
	static const std::string START_TIMEOUT;
	static const std::string READ_RESPONSE_IN_BAD_STATE;
	static const std::string UNSUCCESSFULL_READ_RESPONSE;
	static const std::string UNSUCCESSFULL_START;
	static const std::string CONNECT_IN_NOT_NULL;
	static const std::string ENROLLMENT_NOT_ALLOWED;
	static const std::string START_ENROLLMENT_TIMEOUT;
	static const std::string STOP_ENROLLMENT_RESPONSE_TIMEOUT;
	static const std::string STOP_RESPONSE_IN_BAD_STATE;

	enum State {
		STATE_NULL,
		STATE_WAIT_CONNECT_RESPONSE,
		STATE_WAIT_START_ENROLLMENT_RESPONSE,
		STATE_WAIT_READ_RESPONSE,
		STATE_WAIT_START,
		STATE_ENROLLED,
		STATE_WAIT_START_ENROLLMENT,
		STATE_WAIT_STOP_ENROLLMENT_RESPONSE
	};

	/// Called by the EnrollmentTask when it got an M_RELEASE message
	/// @param cdapMessage
	/// @param cdapSessionDescriptor
	void release(rina::CDAPMessage * cdapMessage,
			rina::CDAPSessionDescriptor * cdapSessionDescriptor);

	/// Called by the EnrollmentTask when it got an M_RELEASE_R message
	/// @param cdapMessage
	/// @param cdapSessionDescriptor
	void releaseResponse(rina::CDAPMessage * cdapMessage,
				rina::CDAPSessionDescriptor * cdapSessionDescriptor);

	/// Called by the EnrollmentTask when the flow supporting the CDAP session with the remote peer
	/// has been deallocated
	/// @param cdapSessionDescriptor
	void flowDeallocated(rina::CDAPSessionDescriptor * cdapSessionDescriptor);

protected:
	BaseEnrollmentStateMachine(IPCProcess * ipc_process,
			const rina::ApplicationProcessNamingInformation& remote_naming_info,
			int timeout, rina::ApplicationProcessNamingInformation * supporting_dif_name);
	~BaseEnrollmentStateMachine();
	bool isValidPortId(const rina::CDAPSessionDescriptor * cdapSessionDescriptor);

	/// Called by the enrollment state machine when the enrollment sequence fails
	void abortEnrollment(const rina::ApplicationProcessNamingInformation& remotePeerNamingInfo,
			int portId, const std::string& reason, bool enrollee, bool sendReleaseMessage);

	/// Create or update the neighbor information in the RIB
	/// @param enrolled true if the neighbor is enrolled, false otherwise
	void createOrUpdateNeighborInformation(bool enrolled);

	/// Sends all the DIF dynamic information
	void sendDIFDynamicInformation();

	/// Gets the object value from the RIB and send it as a CDAP Mesage
	/// @param objectClass the class of the object to be send
	/// @param objectName the name of the object to be send
	void sendCreateInformation(const std::string& objectClass, const std::string& objectName);

	State state_;
	IPCProcess * ipc_process_;
	IRIBDaemon * rib_daemon_;
	rina::CDAPSessionManagerInterface * cdap_session_manager_;
	Encoder * encoder_;
	IEnrollmentTask * enrollment_task_;
	int timeout_;
	rina::Timer * timer_;
	rina::Lockable * lock_;
	int port_id_;
	rina::Neighbor * remote_peer_;
	rina::TimerTask * last_scheduled_task_;
};

/// The state machine of the party that wants to
/// become a new member of the DIF.
class EnrolleeStateMachine: public BaseEnrollmentStateMachine {
public:
	EnrolleeStateMachine(IPCProcess * ipc_process,
			const rina::ApplicationProcessNamingInformation& remote_naming_info,
			int timeout);
	~EnrolleeStateMachine();

	/// Called by the DIFMembersSetObject to initiate the enrollment sequence
	/// with a remote IPC Process
	/// @param enrollment request contains information on the neighbor and on the
	/// enrollment request event
	/// @param portId
	void initiateEnrollment(EnrollmentRequest * enrollmentRequest, int portId);

	/// Called by the EnrollmentTask when it got an M_CONNECT_R message
	/// @param cdapMessage
	/// @param cdapSessionDescriptor
	void connectResponse(rina::CDAPMessage * cdapMessage,
			rina::CDAPSessionDescriptor * cdapSessionDescriptor);

	void startResponse(const rina::CDAPMessage * cdapMessage,
			rina::CDAPSessionDescriptor * cdapSessionDescriptor);

	/// Stop enrollment request received. Check if I have enough information, if not
	/// ask for more with M_READs.
	/// Have to check if I can start operating (if not wait
	/// until M_START operationStatus). If I can start and have enough information,
	/// create or update all the objects received during the enrollment phase.
	void stop(const rina::CDAPMessage * cdapMessage,
			rina::CDAPSessionDescriptor * cdapSessionDescriptor);

	/// See if the response is valid and contains an object. See if more objects
	/// are required. If not, start
	void readResponse(const rina::CDAPMessage * cdapMessage,
			rina::CDAPSessionDescriptor * cdapSessionDescriptor);

	void start(const rina::CDAPMessage * cdapMessage,
				rina::CDAPSessionDescriptor * cdapSessionDescriptor);

private:
	/// See if more information is required for enrollment, or if we can
	/// start or if we have to wait for the start message
	void requestMoreInformationOrStart();

	/// Checks if more information is required for enrollment
	/// (At least there must be DataTransferConstants, a QoS cube and a DAF Member). If there is,
	/// it returns a CDAP READ message requesting the next object to be read. If not, it returns null
	/// @return A CDAP READ message requesting the next object to be read. If not, it returns null
	const rina::CDAPMessage * nextObjectRequired() const;

	/// Create the objects in the RIB
	void commitEnrollment();

	void enrollmentCompleted();

	EnrollmentRequest * enrollment_request_;
	bool was_dif_member_before_enrollment_;
	bool allowed_to_start_early_;
	const rina::CDAPMessage * stop_enrollment_request_message_;
};

/// The state machine of the party that is a member of the DIF
/// and will help the joining party (enrollee) to join the DIF
class EnrollerStateMachine: public BaseEnrollmentStateMachine {
public:
	EnrollerStateMachine(IPCProcess * ipc_process,
		const rina::ApplicationProcessNamingInformation& remote_naming_info, int timeout,
		rina::ApplicationProcessNamingInformation * supporting_dif_name);
	~EnrollerStateMachine();

	/// An M_CONNECT message has been received.  Handle the transition from the
	/// NULL to the WAIT_START_ENROLLMENT state.
	/// Authenticate the remote peer and issue a connect response
	/// @param cdapMessage
    /// @param portId
	void connect(const rina::CDAPMessage * cdapMessage, int portId);

	/// Called by the Enrollment object when it receives an M_START message from
	/// the enrolling member. Have to look at the enrollment information request,
	/// from that deduce if the IPC process requesting to enroll with me is already
	/// a member of the DIF and if its address is valid. If it is not a member of
	/// the DIF, send a new address with the M_START_R, send the M_CREATEs to provide
	/// the DIF initialization information and state, and send M_STOP_R. If it is a
	/// valid member, just send M_START_R with no address and M_STOP_R
	/// @param cdapMessage
    /// @param cdapSessionDescriptor
    void start(const rina::CDAPMessage * cdapMessage,
			const rina::CDAPSessionDescriptor * cdapSessionDescriptor);

    /// The response of the stop operation has been received, send M_START operation without
    /// waiting for an answer and consider the process enrolled
    /// @param cdapMessage
    /// @param cdapSessionDescriptor
	void stopResponse(const rina::CDAPMessage * cdapMessage,
			rina::CDAPSessionDescriptor * cdapSessionDescriptor);

private:
    /// Send a negative response to the M_START enrollment message
    /// @param result the error code
    /// @param resultReason the reason of the bad result
    /// @param requestMessage the received M_START enrollment message
    void sendNegativeStartResponseAndAbortEnrollment(int result, const std::string&
    		resultReason, const rina::CDAPMessage * requestMessage);

    /// Send all the information required to start operation to
    /// the IPC process that is enrolling to me
    void sendDIFStaticInformation();

    void enrollmentCompleted();

	ISecurityManager * security_manager_;
	INamespaceManager * namespace_manager_;
};

/// Handles the operations related to the "daf.management.operationalStatus" object
/*class OperationalStatusRIBObject: public BaseRIBObject {
public:

};*/

}

#endif

#endif
