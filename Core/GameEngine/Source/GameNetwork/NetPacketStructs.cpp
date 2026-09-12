/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#include "PreRTS.h"
#include "GameNetwork/NetPacketStructs.h"

#include "Common/EvolutionCommandCodec.h"
#include "Common/EvolutionGameMessageAdapter.h"
#include "GameNetwork/NetCommandRef.h"

////////////////////////////////////////////////////////////////////////////////

size_t SmallNetPacketCommandBase::getSize(const SmallNetPacketCommandBaseSelect *select)
{
	size_t size = 0;

	if (select != nullptr)
	{
		if (select->useCommandType)
		{
			size += sizeof(NetPacketCommandTypeField);
		}
		if (select->useRelay)
		{
			size += sizeof(NetPacketRelayField);
		}
		if (select->useFrame)
		{
			size += sizeof(NetPacketFrameField);
		}
		if (select->usePlayerId)
		{
			size += sizeof(NetPacketPlayerIdField);
		}
		if (select->useCommandId)
		{
			size += sizeof(NetPacketCommandIdField);
		}
		size += sizeof(NetPacketDataField);
	}
	else
	{
		size += sizeof(SmallNetPacketCommandBase::CommandBase);
	}
	return size;
}

size_t SmallNetPacketCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref, const SmallNetPacketCommandBaseSelect *select)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	size_t size = 0;

	if (select != nullptr)
	{
		// If necessary, put the command type into the packet.
		if (select->useCommandType)
		{
			size += network::writePrimitive(buffer + size, base.commandType);
		}
		// If necessary, put the relay into the packet.
		if (select->useRelay)
		{
			size += network::writePrimitive(buffer + size, base.relay);
		}
		// If necessary, put the execution frame into the packet.
		if (select->useFrame)
		{
			size += network::writePrimitive(buffer + size, base.frame);
		}
		// If necessary, put the player ID into the packet.
		if (select->usePlayerId)
		{
			size += network::writePrimitive(buffer + size, base.playerId);
		}
		// If necessary, put the command ID into the packet.
		if (select->useCommandId)
		{
			size += network::writePrimitive(buffer + size, base.commandId);
		}
		// Always write the data header and mark the end of the command.
		size += network::writePrimitive(buffer + size, base.dataHeader);
	}
	else
	{
		size += network::writeObject(buffer + size, base);
	}
	return size;
}

size_t SmallNetPacketCommandBase::readMessage(NetCommandRef *&ref, CommandBase &base, NetPacketBuf buf)
{
	size_t size = 0;

	while (size < buf.size())
	{
		switch (buf[size])
		{
		case NetPacketFieldTypes::CommandType:
			size += network::readObject(base.commandType, buf.offset(size));
			break;
		case NetPacketFieldTypes::Relay:
			size += network::readObject(base.relay, buf.offset(size));
			break;
		case NetPacketFieldTypes::Frame:
			size += network::readObject(base.frame, buf.offset(size));
			break;
		case NetPacketFieldTypes::PlayerId:
			size += network::readObject(base.playerId, buf.offset(size));
			break;
		case NetPacketFieldTypes::CommandId:
			size += network::readObject(base.commandId, buf.offset(size));
			break;
		case NetPacketFieldTypes::Data:
		{
			size += network::readObject(base.dataHeader, buf.offset(size));
			// The data field marks the end of the command base.
			if (NetCommandMsg* msg = constructNetCommandMsg(base))
			{
				ref = NEW_NETCOMMANDREF(msg);
				ref->setRelay(base.relay.relay);
				msg->detach();
			}
			return size;
		}
		case NetPacketFieldTypes::Repeat:
		default:
			DEBUG_CRASH(("SmallNetPacketCommandBase::readBytes: Unexpected field type '%c' encountered.", buf[size]));
			return size + 1;
		}
	}

	return size;
}

NetCommandMsg *SmallNetPacketCommandBase::constructNetCommandMsg(const CommandBase &base)
{
	NetCommandMsg *msg = nullptr;
	NetCommandType commandType = static_cast<NetCommandType>(base.commandType.commandType);

	switch (commandType)
	{
	case NETCOMMANDTYPE_GAMECOMMAND:
		msg = newInstance(NetGameCommandMsg);
		break;
	case NETCOMMANDTYPE_ACKBOTH:
		msg = newInstance(NetAckBothCommandMsg);
		break;
	case NETCOMMANDTYPE_ACKSTAGE1:
		msg = newInstance(NetAckStage1CommandMsg);
		break;
	case NETCOMMANDTYPE_ACKSTAGE2:
		msg = newInstance(NetAckStage2CommandMsg);
		break;
	case NETCOMMANDTYPE_FRAMEINFO:
		msg = newInstance(NetFrameCommandMsg);
		break;
	case NETCOMMANDTYPE_PLAYERLEAVE:
		msg = newInstance(NetPlayerLeaveCommandMsg);
		break;
	case NETCOMMANDTYPE_RUNAHEADMETRICS:
		msg = newInstance(NetRunAheadMetricsCommandMsg);
		break;
	case NETCOMMANDTYPE_RUNAHEAD:
		msg = newInstance(NetRunAheadCommandMsg);
		break;
	case NETCOMMANDTYPE_DESTROYPLAYER:
		msg = newInstance(NetDestroyPlayerCommandMsg);
		break;
	case NETCOMMANDTYPE_KEEPALIVE:
		msg = newInstance(NetKeepAliveCommandMsg);
		break;
	case NETCOMMANDTYPE_DISCONNECTKEEPALIVE:
		msg = newInstance(NetDisconnectKeepAliveCommandMsg);
		break;
	case NETCOMMANDTYPE_DISCONNECTPLAYER:
		msg = newInstance(NetDisconnectPlayerCommandMsg);
		break;
	case NETCOMMANDTYPE_PACKETROUTERQUERY:
		msg = newInstance(NetPacketRouterQueryCommandMsg);
		break;
	case NETCOMMANDTYPE_PACKETROUTERACK:
		msg = newInstance(NetPacketRouterAckCommandMsg);
		break;
	case NETCOMMANDTYPE_DISCONNECTCHAT:
		msg = newInstance(NetDisconnectChatCommandMsg);
		break;
	case NETCOMMANDTYPE_DISCONNECTVOTE:
		msg = newInstance(NetDisconnectVoteCommandMsg);
		break;
	case NETCOMMANDTYPE_CHAT:
		msg = newInstance(NetChatCommandMsg);
		break;
	case NETCOMMANDTYPE_PROGRESS:
		msg = newInstance(NetProgressCommandMsg);
		break;
	case NETCOMMANDTYPE_LOADCOMPLETE:
		msg = newInstance(NetLoadCompleteCommandMsg);
		break;
	case NETCOMMANDTYPE_TIMEOUTSTART:
		msg = newInstance(NetTimeOutGameStartCommandMsg);
		break;
	case NETCOMMANDTYPE_WRAPPER:
		msg = newInstance(NetWrapperCommandMsg);
		break;
	case NETCOMMANDTYPE_FILE:
		msg = newInstance(NetFileCommandMsg);
		break;
	case NETCOMMANDTYPE_FILEANNOUNCE:
		msg = newInstance(NetFileAnnounceCommandMsg);
		break;
	case NETCOMMANDTYPE_FILEPROGRESS:
		msg = newInstance(NetFileProgressCommandMsg);
		break;
	case NETCOMMANDTYPE_DISCONNECTFRAME:
		msg = newInstance(NetDisconnectFrameCommandMsg);
		break;
	case NETCOMMANDTYPE_DISCONNECTSCREENOFF:
		msg = newInstance(NetDisconnectScreenOffCommandMsg);
		break;
	case NETCOMMANDTYPE_FRAMERESENDREQUEST:
		msg = newInstance(NetFrameResendRequestCommandMsg);
		break;
	default:
		DEBUG_CRASH(("SmallNetPacketCommandBase::constructNetCommandMsg: Unexpected command type '%d' encountered.", commandType));
		return nullptr;
	}

	DEBUG_ASSERTCRASH(commandType == msg->getNetCommandType(),
		("SmallNetPacketCommandBase::constructNetCommandMsg: Read command type '%d' does not match created command '%d'.", commandType, msg->getNetCommandType()));

	msg->setNetCommandType(static_cast<NetCommandType>(base.commandType.commandType));
	msg->setExecutionFrame(base.frame.frame);
	msg->setPlayerID(base.playerId.playerId);
	msg->setID(base.commandId.commandId);

	return msg;
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketAckCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketAckCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.commandId = cmdMsg->getCommandID();
	data.originalPlayerId = cmdMsg->getOriginalPlayerID();

	return network::writeObject(buffer, data);
}

size_t NetPacketAckCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.commandId = 0;
	data.originalPlayerId = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setCommandID(data.commandId);
	cmdMsg->setOriginalPlayerID(data.originalPlayerId);

	return size;
}

size_t NetPacketAckCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	//base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	//base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketFrameCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketFrameCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.commandCount = cmdMsg->getCommandCount();

	return network::writeObject(buffer, data);
}

size_t NetPacketFrameCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.commandCount = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setCommandCount(data.commandCount);

	return size;
}

size_t NetPacketFrameCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketPlayerLeaveCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketPlayerLeaveCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.leavingPlayerId = cmdMsg->getLeavingPlayerID();

	return network::writeObject(buffer, data);
}

size_t NetPacketPlayerLeaveCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.leavingPlayerId = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setLeavingPlayerID(data.leavingPlayerId);

	return size;
}

size_t NetPacketPlayerLeaveCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketRunAheadMetricsCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketRunAheadMetricsCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.averageLatency = cmdMsg->getAverageLatency();
	data.averageFps = cmdMsg->getAverageFps();

	return network::writeObject(buffer, data);
}

size_t NetPacketRunAheadMetricsCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.averageLatency = 0.2f;
	data.averageFps = 30;

	size_t size = network::readObject(data, buf);
	cmdMsg->setAverageLatency(data.averageLatency);
	cmdMsg->setAverageFps(data.averageFps);

	return size;
}

size_t NetPacketRunAheadMetricsCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketRunAheadCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketRunAheadCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.runAhead = cmdMsg->getRunAhead();
	data.frameRate = cmdMsg->getFrameRate();

	return network::writeObject(buffer, data);
}

size_t NetPacketRunAheadCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.runAhead = 20;
	data.frameRate = 30;

	size_t size = network::readObject(data, buf);
	cmdMsg->setRunAhead(data.runAhead);
	cmdMsg->setFrameRate(data.frameRate);

	return size;
}

size_t NetPacketRunAheadCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketDestroyPlayerCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketDestroyPlayerCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.playerIndex = cmdMsg->getPlayerIndex();

	return network::writeObject(buffer, data);
}

size_t NetPacketDestroyPlayerCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.playerIndex = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setPlayerIndex(data.playerIndex);

	return size;
}

size_t NetPacketDestroyPlayerCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketKeepAliveCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketKeepAliveCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	//base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketDisconnectKeepAliveCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketDisconnectKeepAliveCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	//base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketDisconnectPlayerCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketDisconnectPlayerCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.disconnectSlot = cmdMsg->getDisconnectSlot();
	data.disconnectFrame = cmdMsg->getDisconnectFrame();

	return network::writeObject(buffer, data);
}

size_t NetPacketDisconnectPlayerCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.disconnectSlot = 0;
	data.disconnectFrame = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setDisconnectSlot(data.disconnectSlot);
	cmdMsg->setDisconnectFrame(data.disconnectFrame);

	return size;
}

size_t NetPacketDisconnectPlayerCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketRouterQueryCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketRouterQueryCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	//base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketRouterAckCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketRouterAckCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	//base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketDisconnectVoteCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketDisconnectVoteCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.slot = cmdMsg->getSlot();
	data.voteFrame = cmdMsg->getVoteFrame();

	return network::writeObject(buffer, data);
}

size_t NetPacketDisconnectVoteCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.slot = 0;
	data.voteFrame = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setSlot(data.slot);
	cmdMsg->setVoteFrame(data.voteFrame);

	return size;
}

size_t NetPacketDisconnectVoteCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketChatCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketChatCommandData::getSize(const NetCommandMsg &msg)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(&msg);
	const Int textLength = std::min<Int>(cmdMsg->getText().getLength(), 255);

	size_t size = 0;
	size += sizeof(UnsignedByte);
	size += textLength * sizeof(WideChar);
	size += sizeof(Int);
	return size;
}

size_t NetPacketChatCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	const size_t textLength = std::min<size_t>(cmdMsg->getText().getLength(), 255);

	size_t size = 0;
	size += network::writePrimitive(buffer + size, (UnsignedByte)textLength);
	size += network::writeStringWithoutNull(buffer + size, cmdMsg->getText(), textLength);
	size += network::writePrimitive(buffer + size, (Int)cmdMsg->getPlayerMask());
	return size;
}

size_t NetPacketChatCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	UnsignedByte textLength = 0;
	UnicodeString unitext;
	Int playerMask = 0;

	size_t size = 0;
	size += network::readObject(textLength, buf.offset(size));
	size += network::readStringWithoutNull(unitext, textLength, buf.offset(size));
	size += network::readObject(playerMask, buf.offset(size));

	cmdMsg->setText(unitext);
	cmdMsg->setPlayerMask(playerMask);

	return size;
}

size_t NetPacketChatCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketDisconnectChatCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketDisconnectChatCommandData::getSize(const NetCommandMsg &msg)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(&msg);
	const Int textLength = std::min<Int>(cmdMsg->getText().getLength(), 255);

	size_t size = 0;
	size += sizeof(UnsignedByte);
	size += textLength * sizeof(WideChar);
	return size;
}

size_t NetPacketDisconnectChatCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	const Int textLength = std::min<Int>(cmdMsg->getText().getLength(), 255);

	size_t size = 0;
	size += network::writePrimitive(buffer + size, (UnsignedByte)textLength);
	size += network::writeStringWithoutNull(buffer + size, cmdMsg->getText(), textLength);
	return size;
}

size_t NetPacketDisconnectChatCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	UnsignedByte textLength = 0;
	UnicodeString unitext;

	size_t size = 0;
	size += network::readObject(textLength, buf.offset(size));
	size += network::readStringWithoutNull(unitext, textLength, buf.offset(size));

	cmdMsg->setText(unitext);

	return size;
}

size_t NetPacketDisconnectChatCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	//base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketGameCommand
////////////////////////////////////////////////////////////////////////////////

namespace
{

bool NetGameCommandToEvolutionBytes(const NetGameCommandMsg &cmdMsg, std::vector<std::uint8_t> &bytes)
{
    GameMessage *gmsg = cmdMsg.constructGameMessage();
    if (gmsg == nullptr)
        return false;

    evolution::Command command;
    const bool converted = evolution::gameMessageToEvolutionCommand(*gmsg, command);
    deleteInstance(gmsg);
    return converted && evolution::encodeCommandV1(command, bytes);
}

} // namespace

size_t NetPacketGameCommandData::getSize(const NetCommandMsg &msg)
{
    const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(&msg);
    std::vector<std::uint8_t> bytes;
    if (!NetGameCommandToEvolutionBytes(*cmdMsg, bytes))
    {
        DEBUG_CRASH(("Failed to encode Evolution game command for network sizing."));
        return 0;
    }
    return bytes.size();
}

size_t NetPacketGameCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
    const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
    std::vector<std::uint8_t> bytes;
    if (!NetGameCommandToEvolutionBytes(*cmdMsg, bytes))
    {
        DEBUG_CRASH(("Failed to encode Evolution game command."));
        return 0;
    }
    if (!bytes.empty())
        memcpy(buffer, bytes.data(), bytes.size());
    return bytes.size();
}

size_t NetPacketGameCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
    CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
    evolution::Command command;
    const evolution::DecodeResult result = evolution::decodeCommandV1(buf.data(), buf.size(), command);
    if (!result.ok())
    {
        DEBUG_CRASH(("Invalid Evolution game-command payload (error %d).", static_cast<Int>(result.error)));
        return 0;
    }

    cmdMsg->setGameMessageType(static_cast<GameMessage::Type>(command.messageType));
    GameMessage *gmsg = newInstance(GameMessage)(static_cast<GameMessage::Type>(command.messageType));
    if (gmsg == nullptr || !evolution::appendEvolutionCommandToGameMessage(command, *gmsg))
    {
        if (gmsg != nullptr) deleteInstance(gmsg);
        DEBUG_CRASH(("Failed to adapt Evolution command to GameMessage."));
        return 0;
    }
    const UnsignedByte argumentCount = gmsg->getArgumentCount();
    for (UnsignedByte i = 0; i < argumentCount; ++i)
        cmdMsg->addArgument(gmsg->getArgumentDataType(i), *gmsg->getArgument(i));
    deleteInstance(gmsg);
    return result.bytesConsumed;
}

size_t NetPacketGameCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
    const NetCommandMsg *msg = ref.getCommand();
    CommandBase base;
    base.commandType.commandType = msg->getNetCommandType();
    base.relay.relay = ref.getRelay();
    base.frame.frame = msg->getExecutionFrame();
    base.playerId.playerId = msg->getPlayerID();
    base.commandId.commandId = msg->getID();

    return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketWrapperCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketWrapperCommandData::getSize(const NetCommandMsg &msg)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(&msg);

	size_t size = 0;
	size += sizeof(FixedData);
	size += cmdMsg->getDataLength();
	return size;
}

size_t NetPacketWrapperCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.wrappedCommandId = cmdMsg->getWrappedCommandID();
	data.chunkNumber = cmdMsg->getChunkNumber();
	data.numChunks = cmdMsg->getNumChunks();
	data.totalDataLength = cmdMsg->getTotalDataLength();
	data.dataLength = cmdMsg->getDataLength();
	data.dataOffset = cmdMsg->getDataOffset();

	size_t size = 0;
	size += network::writeObject(buffer + size, data);
	size += network::writeBytes(buffer + size, cmdMsg->getData(), cmdMsg->getDataLength());
	return size;
}

size_t NetPacketWrapperCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.wrappedCommandId = 0;
	data.chunkNumber = 0;
	data.numChunks = 0;
	data.totalDataLength = 0;
	data.dataLength = 0;
	data.dataOffset = 0;

	size_t size = 0;
	size += network::readObject(data, buf.offset(size));

	NetCommandDataChunk dataChunk(data.dataLength);
	size += network::readBytes(dataChunk.data(), dataChunk.size(), buf.offset(size));

	cmdMsg->setWrappedCommandID(data.wrappedCommandId);
	cmdMsg->setChunkNumber(data.chunkNumber);
	cmdMsg->setNumChunks(data.numChunks);
	cmdMsg->setTotalDataLength(data.totalDataLength);
	cmdMsg->setDataOffset(data.dataOffset);
	cmdMsg->setData(dataChunk);

	return size;
}

size_t NetPacketWrapperCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketFileCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketFileCommandData::getSize(const NetCommandMsg &msg)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(&msg);

	size_t size = 0;
	size += cmdMsg->getPortableFilename().getByteCount() + 1;
	size += sizeof(UnsignedInt);
	size += cmdMsg->getFileLength();
	return size;
}

size_t NetPacketFileCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());

	size_t size = 0;
	size += network::writeStringWithNull(buffer + size, cmdMsg->getPortableFilename());
	size += network::writePrimitive(buffer + size, (UnsignedInt)cmdMsg->getFileLength());
	size += network::writeBytes(buffer + size, cmdMsg->getFileData(), cmdMsg->getFileLength());
	return size;
}

size_t NetPacketFileCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	AsciiString filename;
	UnsignedInt dataLength = 0;

	size_t size = 0;
	size += network::readStringWithNull(filename, _MAX_PATH, buf.offset(size));
	size += network::readObject(dataLength, buf.offset(size));

	NetCommandDataChunk dataChunk(dataLength);
	size += network::readBytes(dataChunk.data(), dataChunk.size(), buf.offset(size));

	cmdMsg->setPortableFilename(filename);
	cmdMsg->setFileData(dataChunk);

	return size;
}

size_t NetPacketFileCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketFileAnnounceCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketFileAnnounceCommandData::getSize(const NetCommandMsg &msg)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(&msg);

	size_t size = 0;
	size += cmdMsg->getPortableFilename().getByteCount() + 1;
	size += sizeof(UnsignedShort);
	size += sizeof(UnsignedByte);
	return size;
}

size_t NetPacketFileAnnounceCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());

	size_t size = 0;
	size += network::writeStringWithNull(buffer + size, cmdMsg->getPortableFilename());
	size += network::writePrimitive(buffer + size, (UnsignedShort)cmdMsg->getFileID());
	size += network::writePrimitive(buffer + size, (UnsignedByte)cmdMsg->getPlayerMask());
	return size;
}

size_t NetPacketFileAnnounceCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	AsciiString filename;
	UnsignedShort fileID = 0;
	UnsignedByte playerMask = 0;

	size_t size = 0;
	size += network::readStringWithNull(filename, _MAX_PATH, buf.offset(size));
	size += network::readObject(fileID, buf.offset(size));
	size += network::readObject(playerMask, buf.offset(size));

	cmdMsg->setPortableFilename(filename);
	cmdMsg->setFileID(fileID);
	cmdMsg->setPlayerMask(playerMask);

	return size;
}

size_t NetPacketFileAnnounceCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketFileProgressCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketFileProgressCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.fileId = cmdMsg->getFileID();
	data.progress = cmdMsg->getProgress();

	return network::writeObject(buffer, data);
}

size_t NetPacketFileProgressCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.fileId = 0;
	data.progress = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setFileID(data.fileId);
	cmdMsg->setProgress(data.progress);

	return size;
}

size_t NetPacketFileProgressCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketProgressCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketProgressCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.percentage = cmdMsg->getPercentage();

	return network::writeObject(buffer, data);
}

size_t NetPacketProgressCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.percentage = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setPercentage(data.percentage);

	return size;
}

size_t NetPacketProgressCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	//base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketLoadCompleteCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketLoadCompleteCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketTimeOutGameStartCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketTimeOutGameStartCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketDisconnectFrameCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketDisconnectFrameCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.disconnectFrame = cmdMsg->getDisconnectFrame();

	return network::writeObject(buffer, data);
}

size_t NetPacketDisconnectFrameCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.disconnectFrame = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setDisconnectFrame(data.disconnectFrame);

	return size;
}

size_t NetPacketDisconnectFrameCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketDisconnectScreenOffCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketDisconnectScreenOffCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.newFrame = cmdMsg->getNewFrame();

	return network::writeObject(buffer, data);
}

size_t NetPacketDisconnectScreenOffCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.newFrame = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setNewFrame(data.newFrame);

	return size;
}

size_t NetPacketDisconnectScreenOffCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}

////////////////////////////////////////////////////////////////////////////////
// NetPacketFrameResendRequestCommand
////////////////////////////////////////////////////////////////////////////////

size_t NetPacketFrameResendRequestCommandData::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const CommandMsg *cmdMsg = static_cast<const CommandMsg *>(ref.getCommand());
	FixedData data;
	data.frameToResend = cmdMsg->getFrameToResend();

	return network::writeObject(buffer, data);
}

size_t NetPacketFrameResendRequestCommandData::readMessage(NetCommandRef &ref, NetPacketBuf buf)
{
	CommandMsg *cmdMsg = static_cast<CommandMsg *>(ref.getCommand());
	FixedData data;
	data.frameToResend = 0;

	size_t size = network::readObject(data, buf);
	cmdMsg->setFrameToResend(data.frameToResend);

	return size;
}

size_t NetPacketFrameResendRequestCommandBase::copyBytes(UnsignedByte *buffer, const NetCommandRef &ref)
{
	const NetCommandMsg *msg = ref.getCommand();
	CommandBase base;
	base.commandType.commandType = msg->getNetCommandType();
	base.relay.relay = ref.getRelay();
	//base.frame.frame = msg->getExecutionFrame();
	base.playerId.playerId = msg->getPlayerID();
	base.commandId.commandId = msg->getID();

	return network::writeObject(buffer, base);
}
