/*
** Command & Conquer Generals Evolution
** Adapter between runtime GameMessage objects and fixed-width Evolution commands.
*/

#include "Common/EvolutionGameMessageAdapter.h"

#include "Common/MessageStream.h"

#include <cstdint>

namespace evolution
{

static_assert(sizeof(Int) == 4, "Evolution command adapter requires 32-bit Int");
static_assert(sizeof(Real) == 4, "Evolution command adapter requires 32-bit Real");
static_assert(sizeof(ObjectID) == 4, "Evolution command adapter requires 32-bit ObjectID");
static_assert(sizeof(DrawableID) == 4, "Evolution command adapter requires 32-bit DrawableID");
static_assert(static_cast<Int>(ARGUMENTDATATYPE_INTEGER) == static_cast<Int>(CommandArgumentKind::Integer));
static_assert(static_cast<Int>(ARGUMENTDATATYPE_REAL) == static_cast<Int>(CommandArgumentKind::Real));
static_assert(static_cast<Int>(ARGUMENTDATATYPE_BOOLEAN) == static_cast<Int>(CommandArgumentKind::Boolean));
static_assert(static_cast<Int>(ARGUMENTDATATYPE_OBJECTID) == static_cast<Int>(CommandArgumentKind::ObjectId));
static_assert(static_cast<Int>(ARGUMENTDATATYPE_DRAWABLEID) == static_cast<Int>(CommandArgumentKind::DrawableId));
static_assert(static_cast<Int>(ARGUMENTDATATYPE_TEAMID) == static_cast<Int>(CommandArgumentKind::TeamId));
static_assert(static_cast<Int>(ARGUMENTDATATYPE_LOCATION) == static_cast<Int>(CommandArgumentKind::Location));
static_assert(static_cast<Int>(ARGUMENTDATATYPE_PIXEL) == static_cast<Int>(CommandArgumentKind::Pixel));
static_assert(static_cast<Int>(ARGUMENTDATATYPE_PIXELREGION) == static_cast<Int>(CommandArgumentKind::PixelRegion));
static_assert(static_cast<Int>(ARGUMENTDATATYPE_TIMESTAMP) == static_cast<Int>(CommandArgumentKind::Timestamp));
static_assert(static_cast<Int>(ARGUMENTDATATYPE_WIDECHAR) == static_cast<Int>(CommandArgumentKind::WideChar));

bool gameMessageToEvolutionCommand(const GameMessage &message, Command &command)
{
    command = Command{};
    command.messageType = static_cast<Int>(message.getType());

    const UnsignedByte argumentCount = message.getArgumentCount();
    if (static_cast<std::size_t>(argumentCount) > MAX_COMMAND_ARGUMENTS_V1)
        return false;

    command.arguments.reserve(argumentCount);
    for (UnsignedByte i = 0; i < argumentCount; ++i)
    {
        const GameMessageArgumentDataType type = message.getArgumentDataType(i);
        const GameMessageArgumentType *source = message.getArgument(i);
        if (source == nullptr)
            return false;

        CommandArgument arg;
        switch (type)
        {
        case ARGUMENTDATATYPE_INTEGER:
            arg.kind = CommandArgumentKind::Integer;
            arg.signed32 = source->integer;
            break;
        case ARGUMENTDATATYPE_REAL:
            arg.kind = CommandArgumentKind::Real;
            arg.real32[0] = source->real;
            break;
        case ARGUMENTDATATYPE_BOOLEAN:
            arg.kind = CommandArgumentKind::Boolean;
            arg.unsigned32 = source->boolean ? 1U : 0U;
            break;
        case ARGUMENTDATATYPE_OBJECTID:
            arg.kind = CommandArgumentKind::ObjectId;
            arg.signed32 = static_cast<Int>(source->objectID);
            break;
        case ARGUMENTDATATYPE_DRAWABLEID:
            arg.kind = CommandArgumentKind::DrawableId;
            arg.signed32 = static_cast<Int>(source->drawableID);
            break;
        case ARGUMENTDATATYPE_TEAMID:
            arg.kind = CommandArgumentKind::TeamId;
            arg.unsigned32 = source->teamID;
            break;
        case ARGUMENTDATATYPE_LOCATION:
            arg.kind = CommandArgumentKind::Location;
            arg.real32[0] = source->location.x;
            arg.real32[1] = source->location.y;
            arg.real32[2] = source->location.z;
            break;
        case ARGUMENTDATATYPE_PIXEL:
            arg.kind = CommandArgumentKind::Pixel;
            arg.integer32[0] = source->pixel.x;
            arg.integer32[1] = source->pixel.y;
            break;
        case ARGUMENTDATATYPE_PIXELREGION:
            arg.kind = CommandArgumentKind::PixelRegion;
            arg.integer32[0] = source->pixelRegion.lo.x;
            arg.integer32[1] = source->pixelRegion.lo.y;
            arg.integer32[2] = source->pixelRegion.hi.x;
            arg.integer32[3] = source->pixelRegion.hi.y;
            break;
        case ARGUMENTDATATYPE_TIMESTAMP:
            arg.kind = CommandArgumentKind::Timestamp;
            arg.unsigned32 = source->timestamp;
            break;
        case ARGUMENTDATATYPE_WIDECHAR:
        {
            const std::uint32_t wideValue = static_cast<std::uint32_t>(source->wChar);
            if (wideValue > 0xffffU)
                return false;
            arg.kind = CommandArgumentKind::WideChar;
            arg.wideChar16 = static_cast<std::uint16_t>(wideValue);
            break;
        }
        default:
            return false;
        }
        command.arguments.push_back(arg);
    }
    return true;
}

bool appendEvolutionCommandToGameMessage(const Command &command, GameMessage &message)
{
    for (const CommandArgument &source : command.arguments)
    {
        switch (source.kind)
        {
        case CommandArgumentKind::Integer:
            message.appendIntegerArgument(source.signed32);
            break;
        case CommandArgumentKind::Real:
            message.appendRealArgument(source.real32[0]);
            break;
        case CommandArgumentKind::Boolean:
            message.appendBooleanArgument(source.unsigned32 != 0);
            break;
        case CommandArgumentKind::ObjectId:
            message.appendObjectIDArgument(static_cast<ObjectID>(source.signed32));
            break;
        case CommandArgumentKind::DrawableId:
            message.appendDrawableIDArgument(static_cast<DrawableID>(source.signed32));
            break;
        case CommandArgumentKind::TeamId:
            message.appendTeamIDArgument(source.unsigned32);
            break;
        case CommandArgumentKind::Location:
        {
            Coord3D value = {source.real32[0], source.real32[1], source.real32[2]};
            message.appendLocationArgument(value);
            break;
        }
        case CommandArgumentKind::Pixel:
        {
            ICoord2D value = {source.integer32[0], source.integer32[1]};
            message.appendPixelArgument(value);
            break;
        }
        case CommandArgumentKind::PixelRegion:
        {
            IRegion2D value = {{source.integer32[0], source.integer32[1]}, {source.integer32[2], source.integer32[3]}};
            message.appendPixelRegionArgument(value);
            break;
        }
        case CommandArgumentKind::Timestamp:
            message.appendTimestampArgument(source.unsigned32);
            break;
        case CommandArgumentKind::WideChar:
            message.appendWideCharArgument(static_cast<WideChar>(source.wideChar16));
            break;
        default:
            return false;
        }
    }
    return true;
}

} // namespace evolution
