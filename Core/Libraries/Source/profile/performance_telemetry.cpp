/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

#include "rts/profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER) && _MSC_VER < 1300
#include <windows.h>
typedef unsigned __int64 TelemetryTime;
#else
#include <chrono>
typedef unsigned long long TelemetryTime;
#endif

namespace PerformanceTelemetry
{
namespace
{
	FILE *s_captureFile = 0;
	bool s_environmentChecked = false;
	bool s_frameActive = false;
	unsigned int s_captureIndex = 0;
	unsigned int s_renderFrame = 0;
	unsigned int s_syncTimeMs = 0;
	TelemetryTime s_frameStartUs = 0;

	const unsigned int CAPTURE_SCHEMA_VERSION = 1;
	const unsigned int FLUSH_INTERVAL = 120;

	TelemetryTime Get_Time_Microseconds()
	{
#if defined(_MSC_VER) && _MSC_VER < 1300
		LARGE_INTEGER counter;
		LARGE_INTEGER frequency;
		if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0) {
			return 0;
		}
		const TelemetryTime wholeSeconds = (TelemetryTime)(counter.QuadPart / frequency.QuadPart);
		const TelemetryTime remainder = (TelemetryTime)(counter.QuadPart % frequency.QuadPart);
		return wholeSeconds * (TelemetryTime)1000000UL
			+ (remainder * (TelemetryTime)1000000UL) / (TelemetryTime)frequency.QuadPart;
#else
		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		return (TelemetryTime)std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
#endif
	}

	void Write_Header()
	{
		fprintf(s_captureFile,
			"schema_version,capture_index,render_frame,sync_time_ms,render_cpu_us,"
			"draw_calls,triangles,vertices,dx8_triangles,dx8_vertices,skin_draws,"
			"skin_triangles,skin_vertices,sorted_triangles,sorted_vertices,"
			"texture_bytes,texture_count,texture_changes,lightmap_texture_bytes,"
			"lightmap_texture_count,procedural_texture_bytes,procedural_texture_count,"
			"memory_allocations,memory_frees\n");
	}
}

bool Start_Capture(const char *path)
{
	Stop_Capture();
	s_environmentChecked = true;

	if (path == 0 || path[0] == '\0') {
		return false;
	}

	s_captureFile = fopen(path, "w");
	if (s_captureFile == 0) {
		return false;
	}

	s_captureIndex = 0;
	Write_Header();
	return true;
}

bool Start_Capture_From_Environment()
{
	if (s_environmentChecked) {
		return s_captureFile != 0;
	}

	s_environmentChecked = true;
	const char *path = getenv("RTS_PERF_CAPTURE");
	if (path == 0 || path[0] == '\0' || strcmp(path, "0") == 0) {
		return false;
	}

	if (strcmp(path, "1") == 0) {
		path = "RTSPerfCapture.csv";
	}

	// Start_Capture() keeps environment initialization latched.
	return Start_Capture(path);
}

void Stop_Capture()
{
	if (s_captureFile != 0) {
		fflush(s_captureFile);
		fclose(s_captureFile);
		s_captureFile = 0;
	}
	s_frameActive = false;
}

bool Is_Capturing()
{
	return s_captureFile != 0;
}

void Begin_Render_Frame(unsigned int renderFrame, unsigned int syncTimeMs)
{
	Start_Capture_From_Environment();
	s_renderFrame = renderFrame;
	s_syncTimeMs = syncTimeMs;
	s_frameStartUs = Get_Time_Microseconds();
	s_frameActive = true;
}

unsigned long End_Render_Frame(const RenderFrameCounters &counters)
{
	if (!s_frameActive) {
		return 0;
	}

	const TelemetryTime endUs = Get_Time_Microseconds();
	const TelemetryTime elapsedUs64 = endUs >= s_frameStartUs ? endUs - s_frameStartUs : 0;
	const TelemetryTime maxElapsedUs = (TelemetryTime)0xffffffffUL;
	const unsigned long elapsedUs = elapsedUs64 > maxElapsedUs ? 0xffffffffUL : (unsigned long)elapsedUs64;
	s_frameActive = false;

	const unsigned int triangles = counters.dx8Triangles + counters.skinTriangles + counters.sortedTriangles;
	const unsigned int vertices = counters.dx8Vertices + counters.skinVertices + counters.sortedVertices;

	if (s_captureFile != 0) {
		++s_captureIndex;
		fprintf(s_captureFile,
			"%u,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%u,%u,%lu,%u,%lu,%u,%d,%d\n",
			CAPTURE_SCHEMA_VERSION,
			s_captureIndex,
			s_renderFrame,
			s_syncTimeMs,
			elapsedUs,
			counters.drawCalls,
			triangles,
			vertices,
			counters.dx8Triangles,
			counters.dx8Vertices,
			counters.skinDraws,
			counters.skinTriangles,
			counters.skinVertices,
			counters.sortedTriangles,
			counters.sortedVertices,
			counters.textureBytes,
			counters.textureCount,
			counters.textureChanges,
			counters.lightmapTextureBytes,
			counters.lightmapTextureCount,
			counters.proceduralTextureBytes,
			counters.proceduralTextureCount,
			counters.memoryAllocations,
			counters.memoryFrees);

		if ((s_captureIndex % FLUSH_INTERVAL) == 0) {
			fflush(s_captureFile);
		}
	}

	PROFILER_PLOT("Render.CPU.us", (double)elapsedUs);
	PROFILER_PLOT("Render.DrawCalls", (double)counters.drawCalls);
	PROFILER_PLOT("Render.Triangles", (double)triangles);
	PROFILER_PLOT("Render.Vertices", (double)vertices);
	PROFILER_PLOT("Render.TextureBytes", (double)counters.textureBytes);
	PROFILER_PLOT("Render.TextureChanges", (double)counters.textureChanges);
	PROFILER_PLOT("Render.MemoryAllocations", (double)counters.memoryAllocations);
	PROFILER_PLOT("Render.MemoryFrees", (double)counters.memoryFrees);

	return elapsedUs;
}
}
