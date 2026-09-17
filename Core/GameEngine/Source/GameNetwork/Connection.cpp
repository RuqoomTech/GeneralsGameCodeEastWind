/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////


#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameNetwork/Connection.h"
#include "GameNetwork/networkutil.h"
#if defined(_WIN64)
#include "GameNetwork/EvolutionProtocol.h"
#include "Common/EvolutionGameMessageAdapter.h"
#endif
#include "GameLogic/GameLogic.h"

#if defined(_WIN64)
#include <cstdint>
#include <vector>
#endif

enum { MaxQuitFlushTime = 30000 }; // wait this many milliseconds at most to retry things before quitting

#if defined(_WIN64)
namespace
{
Bool QueueEvolutionGameCommand(Transport *transport, const User *user, const NetCommandRef &ref)
{
	if (transport == nullptr || user == nullptr || ref.getCommand() == nullptr ||
		ref.getCommand()->getNetCommandType() != NETCOMMANDTYPE_GAMECOMMAND)
	{
		return FALSE;
	}

	const NetGameCommandMsg *netMessage = static_cast<const NetGameCommandMsg *>(ref.getCommand());
	GameMessage *gameMessage = netMessage->constructGameMessage();
	if (gameMessage == nullptr)
	{
		return FALSE;
	}

	evolution::Command command;
	const bool converted = evolution::gameMessageToEvolutionCommand(*gameMessage, command);
	deleteInstance(gameMessage);
	if (!converted)
	{
		return FALSE;
	}

	evolution::NetworkCommandRecord record;
	record.playerId = static_cast<std::uint8_t>(netMessage->getPlayerID());
	record.relayMask = ref.getRelay();
	record.commandId = netMessage->getID();
	record.command = command;

	std::vector<evolution::NetworkCommandRecord> records(1, record);
	std::vector<std::uint8_t> packet;
	const std::uint32_t sequence = (static_cast<std::uint32_t>(record.playerId) << 16U) | record.commandId;
	if (!evolution::encodeRoutedCommandPacketV1(
			sequence, netMessage->getExecutionFrame(), records, packet) ||
		packet.size() > static_cast<std::size_t>(MAX_NETWORK_MESSAGE_LEN))
	{
		return FALSE;
	}

	return transport->queueEvolutionSend(
		user->GetIPAddr(), user->GetPort(), packet.data(), static_cast<Int>(packet.size()));
}
} // namespace
#endif

/**
 * The constructor.
 */
Connection::Connection() {
	m_transport = nullptr;
	m_user = nullptr;
	m_netCommandList = nullptr;
	m_retryTime = 2000; // set retry time to 2 seconds.
	m_lastTimeSent = 0;
	m_frameGrouping = 1;
	m_isQuitting = false;
	m_quitTime = 0;
	m_averageLatency = 0.0f;
	Int i;
	for(i = 0; i < CONNECTION_LATENCY_HISTORY_LENGTH; i++)
	{
		m_latencies[i] = 0.0f;
	}
}

/**
 * The destructor.
 */
Connection::~Connection() {
	deleteInstance(m_user);
	m_user = nullptr;

	deleteInstance(m_netCommandList);
	m_netCommandList = nullptr;
}

/**
 * Initialize the connection and any subsystems.
 */
void Connection::init() {
	m_transport = nullptr;

	deleteInstance(m_user);
	m_user = nullptr;

	if (m_netCommandList == nullptr) {
		m_netCommandList = newInstance(NetCommandList);
		m_netCommandList->init();
	}
	m_netCommandList->reset();

	m_lastTimeSent = 0;
	m_frameGrouping = 1;
	m_numRetries = 0;
	m_retryMetricsTime = 0;

	for (Int i = 0; i < CONNECTION_LATENCY_HISTORY_LENGTH; ++i) {
		m_latencies[i] = 0;
	}
	m_averageLatency = 0;
	m_isQuitting = FALSE;
	m_quitTime = 0;
}

/**
 * Take the connection back to the initial state.
 */
void Connection::reset() {
	init();
}

/**
 * Doesn't really do anything.
 */
void Connection::update() {
}

/**
 * Attach the transport object that this connection should use.
 */
void Connection::attachTransport(Transport *transport) {
	m_transport = transport;
}

/**
 * Assign this connection a user.  This is the user to whome we send all our packetized goodies.
 */
void Connection::setUser(User *user) {
	deleteInstance(m_user);
	m_user = user;
}

/**
 * Return the user object.
 */
User * Connection::getUser() {
	return m_user;
}

/**
 * Add this network command to the send queue for this connection.
 * The relay is the mask specifying the people the person we are sending to should send to.
 * The relay mostly has to do with the packet router.
 */
void Connection::sendNetCommandMsg(NetCommandMsg *msg, UnsignedByte relay) {
	if (m_isQuitting)
		return;

	if (m_netCommandList != nullptr) {
		NetPacket packet;

		// check to see if this command will fit in a packet.  If not, we need to split it up.
		// we are splitting up the command here so that the retry logic will not try to
		// resend the ENTIRE command (i.e. multiple packets work of data) and only do the retry
		// one wrapper command at a time.

		NetCommandRef *tempref = NEW_NETCOMMANDREF(msg);
		if (packet.addCommand(tempref)) {
			deleteInstance(tempref);
			tempref = nullptr;
		} else {
			tempref->setRelay(relay);

			// the message doesn't fit in a single packet, need to split it up.
			if (NetCommandList* list = NetPacket::ConstructBigCommandList(tempref)) {
				for (NetCommandRef* ref1 = list->getFirstMessage(); ref1 != nullptr; ref1 = ref1->getNext()) {
					if (NetCommandRef* ref2 = m_netCommandList->addMessage(ref1->getCommand())) {
						ref2->setRelay(relay);
					}
				}

				deleteInstance(list);
				list = nullptr;
			}

			deleteInstance(tempref);
			tempref = nullptr;

			return;
		}

		// the message fits in a packet, add to the command list normally.
		NetCommandRef *ref = m_netCommandList->addMessage(msg);

		if (ref != nullptr) {

/*
#if defined(RTS_DEBUG)
			if (msg->getNetCommandType() == NETCOMMANDTYPE_GAMECOMMAND) {
				DEBUG_LOG(("Connection::sendNetCommandMsg - added game command %d to net command list for frame %d.",
					msg->getID(), msg->getExecutionFrame()));
			} else if (msg->getNetCommandType() == NETCOMMANDTYPE_FRAMEINFO) {
				DEBUG_LOG(("Connection::sendNetCommandMsg - added frame info for frame %d", msg->getExecutionFrame()));
			}
#endif // RTS_DEBUG
*/

			ref->setRelay(relay);
		}
	}
}

void Connection::clearCommandsExceptFrom( Int playerIndex )
{
	NetCommandRef *tmp = m_netCommandList->getFirstMessage();
	while (tmp)
	{
		NetCommandRef *next = tmp->getNext();
		NetCommandMsg *msg = tmp->getCommand();

		if (msg->getPlayerID() != playerIndex)
		{
			DEBUG_LOG(("Connection::clearCommandsExceptFrom(%d) - clearing a command from player %d for frame %d",
				playerIndex, tmp->getCommand()->getPlayerID(), tmp->getCommand()->getExecutionFrame()));

			m_netCommandList->removeMessage(tmp);
			deleteInstance(tmp);
		}

		tmp = next;
	}
}

Bool Connection::isQueueEmpty() {
	if (m_netCommandList->getFirstMessage() == nullptr) {
		return TRUE;
	}
	return FALSE;
}

void Connection::setQuitting()
{
	m_isQuitting = TRUE;
	m_quitTime = timeGetTime();
	DEBUG_LOG(("Connection::setQuitting() at time %d", m_quitTime));
}

/**
 * This is the good part. We take all the network commands queued up for this connection,
 * packetize them and put them on the transport's send queue for actual sending.
 */
UnsignedInt Connection::doSend() {
	Int numpackets = 0;
	time_t curtime = timeGetTime();
	Bool couldQueue = TRUE;

	// Do this check first, since it's an important fail-safe
	if (m_isQuitting && curtime > m_quitTime + MaxQuitFlushTime)
	{
		DEBUG_LOG(("Timed out a quitting connection.  Deleting all %d messages", m_netCommandList->length()));
		m_netCommandList->reset();
		return 0;
	}

	if ((curtime - m_lastTimeSent) < m_frameGrouping) {
		return 0;
	}

	NetCommandRef *msg = m_netCommandList->getFirstMessage();

	while ((msg != nullptr) && couldQueue) {
		NetPacket packet;
		packet.setAddress(m_user->GetIPAddr(), m_user->GetPort());

		Bool notDone = TRUE;
		while ((msg != nullptr) && notDone) {
			NetCommandRef *next = msg->getNext();
			time_t timeLastSent = msg->getTimeLastSent();

			if (((curtime - timeLastSent) > m_retryTime) || (timeLastSent == -1)) {
#if defined(_WIN64)
				// Step 04E2: gameplay commands leave the legacy transport wrapper and are
				// sent as routed EVN1 datagrams. The command remains in the existing
				// Connection retry/ACK list, so reliability semantics do not change yet.
				if (msg->getCommand()->getNetCommandType() == NETCOMMANDTYPE_GAMECOMMAND &&
					QueueEvolutionGameCommand(m_transport, m_user, *msg))
				{
					if (CommandRequiresAck(msg->getCommand())) {
						if (timeLastSent != -1) {
							++m_numRetries;
						}
						doRetryMetrics();
						msg->setTimeLastSent(curtime);
					} else {
						m_netCommandList->removeMessage(msg);
						deleteInstance(msg);
					}
					++numpackets;
					m_lastTimeSent = curtime;
					msg = next;
					continue;
				}
#endif

				// Control/reliability traffic and any gameplay command that could not
				// be represented/queued as EVN1 retain the proven legacy packet path.
				notDone = packet.addCommand(msg);
				if (notDone) {
					if (CommandRequiresAck(msg->getCommand())) {
						if (timeLastSent != -1) {
							++m_numRetries;
						}
						doRetryMetrics();
						msg->setTimeLastSent(curtime);
					} else {
						m_netCommandList->removeMessage(msg);
						deleteInstance(msg);
					}
				}
			}
			msg = next;
		}

		if (msg != nullptr) {
			DEBUG_LOG(("didn't finish sending all commands in connection"));
		}

		if (packet.getNumCommands() > 0) {
			couldQueue = m_transport->queueSend(packet.getAddr(), packet.getPort(), packet.getData(), packet.getLength());
			if (couldQueue) {
				++numpackets;
				m_lastTimeSent = curtime;
			}
		}
	}

	return numpackets;
}

NetCommandRef * Connection::processAck(NetAckStage1CommandMsg *msg) {
	return processAck(msg->getCommandID(), msg->getOriginalPlayerID());
}

NetCommandRef * Connection::processAck(NetAckBothCommandMsg *msg) {
	return processAck(msg->getCommandID(), msg->getOriginalPlayerID());
}

NetCommandRef * Connection::processAck(NetCommandMsg *msg) {
	if (msg->getNetCommandType() == NETCOMMANDTYPE_ACKSTAGE1) {
		NetAckStage1CommandMsg *ackmsg = (NetAckStage1CommandMsg *)msg;
		return processAck(ackmsg);
	}

	if (msg->getNetCommandType() == NETCOMMANDTYPE_ACKBOTH) {
		NetAckBothCommandMsg *ackmsg = (NetAckBothCommandMsg *)msg;
		return processAck(ackmsg);
	}

	return nullptr;
}

/**
 * The person we are sending to has ack'd one of the messages we sent him.
 * Take that message off the list of commands to send.
 */
NetCommandRef * Connection::processAck(UnsignedShort commandID, UnsignedByte originalPlayerID) {
	NetCommandRef *temp = m_netCommandList->getFirstMessage();
	while ((temp != nullptr) && ((temp->getCommand()->getID() != commandID) || (temp->getCommand()->getPlayerID() != originalPlayerID))) {

		// cycle through the commands till we find the one we need to remove.
		// Need to check for both the command ID and the player ID.
		temp = temp->getNext();
	}
	if (temp == nullptr) {
		return nullptr;
	}

#if defined(RTS_DEBUG)
	Bool doDebug = FALSE;
	if (temp->getCommand()->getNetCommandType() == NETCOMMANDTYPE_DISCONNECTFRAME) {
		doDebug = TRUE;
	}
#endif

	Int index = temp->getCommand()->getID() % CONNECTION_LATENCY_HISTORY_LENGTH;
	m_averageLatency -= ((Real)(m_latencies[index])) / CONNECTION_LATENCY_HISTORY_LENGTH;
	Real lat = timeGetTime() - temp->getTimeLastSent();
	m_averageLatency += lat / CONNECTION_LATENCY_HISTORY_LENGTH;
	m_latencies[index] = lat;

#if defined(RTS_DEBUG)
	if (doDebug == TRUE) {
		DEBUG_LOG(("Connection::processAck - disconnect frame command %d found, removing from command list.", commandID));
	}
#endif
	m_netCommandList->removeMessage(temp);
	return temp;
}

void Connection::setFrameGrouping(time_t frameGrouping) {
	m_frameGrouping = frameGrouping;
//	m_retryTime = frameGrouping * 4;
}

void Connection::doRetryMetrics() {
	static Int numSeconds = 0;
	time_t curTime = timeGetTime();

	if ((curTime - m_retryMetricsTime) > 10000) {
		m_retryMetricsTime = curTime;
		++numSeconds;
//		DEBUG_LOG(("Retries in the last 10 seconds = %d, average latency = %fms", m_numRetries, m_averageLatency));
		m_numRetries = 0;
//		m_retryTime = m_averageLatency * 1.5;
	}
}

#if defined(RTS_DEBUG)
void Connection::debugPrintCommands() {
	NetCommandRef *ref = m_netCommandList->getFirstMessage();
	while (ref != nullptr) {
		DEBUG_LOG(("Connection::debugPrintCommands - ID: %d\tType: %s\tRelay: 0x%X for frame %d",
			ref->getCommand()->getID(), GetNetCommandTypeAsString(ref->getCommand()->getNetCommandType()),
			ref->getRelay(), ref->getCommand()->getExecutionFrame()));
		ref = ref->getNext();
	}
}
#endif
