//
// Internal events between IPC Process components
//
//    Bernat Gaston <bernat.gaston@i2cat.net>
//    Eduard Grasa <eduard.grasa@i2cat.net>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include <sstream>

#include "events.h"

namespace rinad {

//CLASS BaseEvent
BaseEvent::BaseEvent(const IPCProcessEventType& id) {
	id_ = id;
}

IPCProcessEventType BaseEvent::get_id() const {
	return id_;
}

//CLASS NMinusOneFlowAllocationFailedEvent
NMinusOneFlowAllocationFailedEvent::NMinusOneFlowAllocationFailedEvent(unsigned int handle,
			const rina::FlowInformation& flow_information,
			const std::string& result_reason): BaseEvent(IPCP_EVENT_N_MINUS_1_FLOW_ALLOCATION_FAILED) {
	handle_ = handle;
	flow_information_ = flow_information;
	result_reason_ = result_reason;
}

unsigned int NMinusOneFlowAllocationFailedEvent::get_handle() const {
	return handle_;
}

const rina::FlowInformation& NMinusOneFlowAllocationFailedEvent::get_flow_information() const {
	return flow_information_;
}

const std::string& NMinusOneFlowAllocationFailedEvent::get_result_reason() const {
	return result_reason_;
}

const std::string NMinusOneFlowAllocationFailedEvent::toString() {
	std::stringstream ss;
	ss<<"Event id: "<<get_id()<<"; Handle: "<<handle_;
	ss<<"; Result reason: "<<result_reason_<<std::endl;
	ss<<"Flow description: "<<flow_information_.toString();
	return ss.str();
}

//CLASS NMinusOneFlowAllocatedEvent
NMinusOneFlowAllocatedEvent::NMinusOneFlowAllocatedEvent(unsigned int handle,
			const rina::FlowInformation& flow_information):
					BaseEvent(IPCP_EVENT_N_MINUS_1_FLOW_ALLOCATED) {
	handle_ = handle;
	flow_information_ = flow_information;
}

unsigned int NMinusOneFlowAllocatedEvent::get_handle() const {
	return handle_;
}

const rina::FlowInformation& NMinusOneFlowAllocatedEvent::get_flow_information() const {
	return flow_information_;
}

const std::string NMinusOneFlowAllocatedEvent::toString() {
	std::stringstream ss;
	ss<<"Event id: "<<get_id()<<"; Handle: "<<handle_<<std::endl;
	ss<<"Flow description: "<<flow_information_.toString();
	return ss.str();
}

//CLASS NMinusOneFlowDeallocated Event
NMinusOneFlowDeallocatedEvent::NMinusOneFlowDeallocatedEvent(unsigned int port_id,
			rina::CDAPSessionDescriptor * cdap_session_descriptor):
				BaseEvent(IPCP_EVENT_N_MINUS_1_FLOW_DEALLOCATED) {
	port_id_ = port_id;
	cdap_session_descriptor_ = cdap_session_descriptor;
}

unsigned int NMinusOneFlowDeallocatedEvent::get_port_id() const {
	return port_id_;
}

rina::CDAPSessionDescriptor * NMinusOneFlowDeallocatedEvent::get_cdap_session_descriptor() const {
	return cdap_session_descriptor_;
}

const std::string NMinusOneFlowDeallocatedEvent::toString() {
	std::stringstream ss;
	ss<<"Event id: "<<get_id()<<"; Port-id: "<<port_id_<<std::endl;
	return ss.str();
}

//CLASS Connectivity to Neighbor lost
ConnectiviyToNeighborLostEvent::ConnectiviyToNeighborLostEvent(rina::Neighbor* neighbor):
		BaseEvent(IPCP_EVENT_CONNECTIVITY_TO_NEIGHBOR_LOST) {
	neighbor_ = neighbor;
}

rina::Neighbor * ConnectiviyToNeighborLostEvent::get_neighbor() {
	return neighbor_;
}

const std::string ConnectiviyToNeighborLostEvent::toString() {
	std::stringstream ss;
	ss<<"Event id: "<<get_id()<<"; Neighbor: "<<neighbor_->toString()<<std::endl;
	return ss.str();
}

}
