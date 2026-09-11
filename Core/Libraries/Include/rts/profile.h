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

/////////////////////////////////////////////////////////////////////////EA-V1
// $File: //depot/GeneralsMD/Staging/code/Libraries/Include/rts/profile.h $
// $Author: mhoffe $
// $Revision: #1 $
// $DateTime: 2003/07/03 11:55:26 $
//
// (c) 2003 Electronic Arts
//
// Proxy header for profile module
//////////////////////////////////////////////////////////////////////////////

#pragma once

#if defined(RTS_PROFILE_LEGACY)
#include "../../Source/profile/profile.h"
#endif

#if defined(RTS_PROFILE_TRACY)

#include <tracy/Tracy.hpp>

#define PROFILER_ENABLED
#define PROFILER_FRAME_IMAGE_SIZE 256 // Horizontal size of the frame image in pixels.
#define PROFILER_FRAME_IMAGE_INTERVAL_MS 500 // Will capture every render frame if set to 0
#define PROFILER_SECTION ZoneScoped
#define PROFILER_SECTION_NAME(name) ZoneScopedN(name)
#define PROFILER_SECTION_COLOR(color) ZoneScopedC(color)
#define PROFILER_SECTION_NAMECOLOR(name, color) ZoneScopedNC(name, color)
#define PROFILER_FRAME_MARK FrameMark
#define PROFILER_FRAME_MARK_NAME(name) FrameMarkNamed(name)
#define PROFILER_FRAME_IMAGE(image, width, height, offset, flip) FrameImage(image, width, height, offset, flip)
#define PROFILER_MSG(txt, size) TracyMessage(txt, size)
#define PROFILER_PLOT(name, value) TracyPlot(name, value)
#define PROFILER_IS_CONNECTED TracyIsConnected

#else

#define PROFILER_FRAME_IMAGE_SIZE 0
#define PROFILER_FRAME_IMAGE_INTERVAL_MS 0
#define PROFILER_SECTION ((void)0)
#define PROFILER_SECTION_NAME(name) ((void)0)
#define PROFILER_SECTION_COLOR(color) ((void)0)
#define PROFILER_SECTION_NAMECOLOR(name, color) ((void)0)
#define PROFILER_FRAME_MARK ((void)0)
#define PROFILER_FRAME_MARK_NAME(name) ((void)0)
#define PROFILER_FRAME_IMAGE(image, width, height, offset, flip) ((void)0)
#define PROFILER_MSG(txt, size) ((void)0)
#define PROFILER_PLOT(name, value) ((void)0)
#define PROFILER_IS_CONNECTED false

#endif
// Step 03 performance telemetry. This remains independent of both the legacy
// profiler and Tracy so the engine can publish one stable observational sample
// to either CSV capture or optional profiler plots. None of these values may
// feed simulation, replay, networking, CRC, frame pacing, or gameplay decisions.
namespace PerformanceTelemetry
{
	enum UpdatePhase
	{
		UPDATE_PHASE_CLIENT = 0,
		UPDATE_PHASE_MESSAGE_STREAM,
		UPDATE_PHASE_NETWORK,
		UPDATE_PHASE_LOGIC,
		UPDATE_PHASE_COUNT
	};

	struct RenderFrameCounters
	{
		unsigned int drawCalls;
		unsigned int dx8Triangles;
		unsigned int dx8Vertices;
		unsigned int skinDraws;
		unsigned int skinTriangles;
		unsigned int skinVertices;
		unsigned int sortedTriangles;
		unsigned int sortedVertices;
		unsigned long textureBytes;
		unsigned int textureCount;
		unsigned int textureChanges;
		unsigned long lightmapTextureBytes;
		unsigned int lightmapTextureCount;
		unsigned long proceduralTextureBytes;
		unsigned int proceduralTextureCount;
		int memoryAllocations;
		int memoryFrees;
	};

	// Explicit capture is useful for tests/tools. Runtime profile builds normally
	// use RTS_PERF_CAPTURE and let the first update/render frame initialize lazily.
	bool Start_Capture(const char *path);
	bool Start_Capture_From_Environment();
	void Stop_Capture();
	bool Is_Capturing();

	// Step 03B update-phase timing. Begin/End_Update_Frame bracket one
	// GameEngine::update() call. Phase timing is additive within that frame.
	void Begin_Update_Frame(unsigned int logicFrame);
	void Begin_Update_Phase(UpdatePhase phase);
	unsigned long End_Update_Phase(UpdatePhase phase);
	void Record_Drawable_Visibility(unsigned int total, unsigned int visible, unsigned int shrouded);
	unsigned long End_Update_Frame();

	// Primary WW3D render bracket. When an update frame is active these values are
	// buffered into that row; standalone renderer/tests still emit a render-only row.
	void Begin_Render_Frame(unsigned int renderFrame, unsigned int syncTimeMs);
	unsigned long End_Render_Frame(const RenderFrameCounters &counters);
}
