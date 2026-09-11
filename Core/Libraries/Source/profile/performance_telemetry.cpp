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
	struct FrameSample
	{
		unsigned int renderFrame;
		unsigned int logicFrame;
		unsigned int syncTimeMs;
		unsigned int rendered;
		unsigned int logicUpdated;
		unsigned long updateCpuUs;
		unsigned long phaseCpuUs[UPDATE_PHASE_COUNT];
		unsigned long renderCpuUs;
		unsigned int drawableTotal;
		unsigned int drawableVisible;
		unsigned int drawableShrouded;
		RenderFrameCounters render;
	};

	FILE *s_captureFile = 0;
	bool s_environmentChecked = false;
	bool s_updateActive = false;
	bool s_renderActive = false;
	unsigned int s_captureIndex = 0;
	TelemetryTime s_updateStartUs = 0;
	TelemetryTime s_renderStartUs = 0;
	TelemetryTime s_phaseStartUs[UPDATE_PHASE_COUNT] = { 0 };
	bool s_phaseActive[UPDATE_PHASE_COUNT] = { false };
	FrameSample s_sample;

	const unsigned int CAPTURE_SCHEMA_VERSION = 2;
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

	unsigned long Elapsed_Microseconds(TelemetryTime startUs, TelemetryTime endUs)
	{
		const TelemetryTime elapsedUs64 = endUs >= startUs ? endUs - startUs : 0;
		const TelemetryTime maxElapsedUs = (TelemetryTime)0xffffffffUL;
		return elapsedUs64 > maxElapsedUs ? 0xffffffffUL : (unsigned long)elapsedUs64;
	}

	void Reset_Sample()
	{
		memset(&s_sample, 0, sizeof(s_sample));
		memset(s_phaseStartUs, 0, sizeof(s_phaseStartUs));
		memset(s_phaseActive, 0, sizeof(s_phaseActive));
	}

	void Write_Header()
	{
		fprintf(s_captureFile,
			"schema_version,capture_index,render_frame,logic_frame,sync_time_ms,rendered,logic_updated,"
			"update_cpu_us,client_cpu_us,logic_cpu_us,network_cpu_us,message_cpu_us,render_cpu_us,"
			"drawable_total,drawable_visible,drawable_shrouded,"
			"draw_calls,triangles,vertices,dx8_triangles,dx8_vertices,skin_draws,"
			"skin_triangles,skin_vertices,sorted_triangles,sorted_vertices,"
			"texture_bytes,texture_count,texture_changes,lightmap_texture_bytes,"
			"lightmap_texture_count,procedural_texture_bytes,procedural_texture_count,"
			"memory_allocations,memory_frees\n");
	}

	void Publish_Plots()
	{
		const unsigned int triangles = s_sample.render.dx8Triangles
			+ s_sample.render.skinTriangles + s_sample.render.sortedTriangles;
		const unsigned int vertices = s_sample.render.dx8Vertices
			+ s_sample.render.skinVertices + s_sample.render.sortedVertices;
		// Non-Tracy builds compile PROFILER_PLOT to a no-op that does not consume
		// macro arguments; keep the derived values warning-clean in that configuration.
		(void)triangles;
		(void)vertices;

		PROFILER_PLOT("Update.CPU.us", (double)s_sample.updateCpuUs);
		PROFILER_PLOT("Update.Client.us", (double)s_sample.phaseCpuUs[UPDATE_PHASE_CLIENT]);
		PROFILER_PLOT("Update.Logic.us", (double)s_sample.phaseCpuUs[UPDATE_PHASE_LOGIC]);
		PROFILER_PLOT("Update.Network.us", (double)s_sample.phaseCpuUs[UPDATE_PHASE_NETWORK]);
		PROFILER_PLOT("Render.CPU.us", (double)s_sample.renderCpuUs);
		PROFILER_PLOT("Render.DrawCalls", (double)s_sample.render.drawCalls);
		PROFILER_PLOT("Render.Triangles", (double)triangles);
		PROFILER_PLOT("Render.Vertices", (double)vertices);
		PROFILER_PLOT("Render.TextureBytes", (double)s_sample.render.textureBytes);
		PROFILER_PLOT("Render.TextureChanges", (double)s_sample.render.textureChanges);
		PROFILER_PLOT("Render.MemoryAllocations", (double)s_sample.render.memoryAllocations);
		PROFILER_PLOT("Render.MemoryFrees", (double)s_sample.render.memoryFrees);
		PROFILER_PLOT("Client.Drawables.Total", (double)s_sample.drawableTotal);
		PROFILER_PLOT("Client.Drawables.Visible", (double)s_sample.drawableVisible);
		PROFILER_PLOT("Client.Drawables.Shrouded", (double)s_sample.drawableShrouded);
	}

	void Publish_Sample()
	{
		const unsigned int triangles = s_sample.render.dx8Triangles
			+ s_sample.render.skinTriangles + s_sample.render.sortedTriangles;
		const unsigned int vertices = s_sample.render.dx8Vertices
			+ s_sample.render.skinVertices + s_sample.render.sortedVertices;

		if (s_captureFile != 0) {
			++s_captureIndex;
			fprintf(s_captureFile,
				"%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu,%u,%u,%u,"
				"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%u,%u,%lu,%u,%lu,%u,%d,%d\n",
				CAPTURE_SCHEMA_VERSION,
				s_captureIndex,
				s_sample.renderFrame,
				s_sample.logicFrame,
				s_sample.syncTimeMs,
				s_sample.rendered,
				s_sample.logicUpdated,
				s_sample.updateCpuUs,
				s_sample.phaseCpuUs[UPDATE_PHASE_CLIENT],
				s_sample.phaseCpuUs[UPDATE_PHASE_LOGIC],
				s_sample.phaseCpuUs[UPDATE_PHASE_NETWORK],
				s_sample.phaseCpuUs[UPDATE_PHASE_MESSAGE_STREAM],
				s_sample.renderCpuUs,
				s_sample.drawableTotal,
				s_sample.drawableVisible,
				s_sample.drawableShrouded,
				s_sample.render.drawCalls,
				triangles,
				vertices,
				s_sample.render.dx8Triangles,
				s_sample.render.dx8Vertices,
				s_sample.render.skinDraws,
				s_sample.render.skinTriangles,
				s_sample.render.skinVertices,
				s_sample.render.sortedTriangles,
				s_sample.render.sortedVertices,
				s_sample.render.textureBytes,
				s_sample.render.textureCount,
				s_sample.render.textureChanges,
				s_sample.render.lightmapTextureBytes,
				s_sample.render.lightmapTextureCount,
				s_sample.render.proceduralTextureBytes,
				s_sample.render.proceduralTextureCount,
				s_sample.render.memoryAllocations,
				s_sample.render.memoryFrees);

			if ((s_captureIndex % FLUSH_INTERVAL) == 0) {
				fflush(s_captureFile);
			}
		}

		Publish_Plots();
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

	return Start_Capture(path);
}

void Stop_Capture()
{
	if (s_captureFile != 0) {
		fflush(s_captureFile);
		fclose(s_captureFile);
		s_captureFile = 0;
	}
	s_updateActive = false;
	s_renderActive = false;
	Reset_Sample();
}

bool Is_Capturing()
{
	return s_captureFile != 0;
}

void Begin_Update_Frame(unsigned int logicFrame)
{
	Start_Capture_From_Environment();
	Reset_Sample();
	s_sample.logicFrame = logicFrame;
	s_updateStartUs = Get_Time_Microseconds();
	s_updateActive = true;
	s_renderActive = false;
}

void Begin_Update_Phase(UpdatePhase phase)
{
	if (!s_updateActive || phase < 0 || phase >= UPDATE_PHASE_COUNT) {
		return;
	}

	s_phaseStartUs[phase] = Get_Time_Microseconds();
	s_phaseActive[phase] = true;
}

unsigned long End_Update_Phase(UpdatePhase phase)
{
	if (!s_updateActive || phase < 0 || phase >= UPDATE_PHASE_COUNT || !s_phaseActive[phase]) {
		return 0;
	}

	const unsigned long elapsedUs = Elapsed_Microseconds(s_phaseStartUs[phase], Get_Time_Microseconds());
	s_sample.phaseCpuUs[phase] += elapsedUs;
	s_phaseActive[phase] = false;
	if (phase == UPDATE_PHASE_LOGIC) {
		s_sample.logicUpdated = 1;
	}
	return elapsedUs;
}

void Record_Drawable_Visibility(unsigned int total, unsigned int visible, unsigned int shrouded)
{
	if (!s_updateActive) {
		return;
	}

	s_sample.drawableTotal = total;
	s_sample.drawableVisible = visible;
	s_sample.drawableShrouded = shrouded;
}

unsigned long End_Update_Frame()
{
	if (!s_updateActive) {
		return 0;
	}

	for (int phase = 0; phase < UPDATE_PHASE_COUNT; ++phase) {
		if (s_phaseActive[phase]) {
			End_Update_Phase((UpdatePhase)phase);
		}
	}

	s_sample.updateCpuUs = Elapsed_Microseconds(s_updateStartUs, Get_Time_Microseconds());
	s_updateActive = false;
	Publish_Sample();
	return s_sample.updateCpuUs;
}

void Begin_Render_Frame(unsigned int renderFrame, unsigned int syncTimeMs)
{
	Start_Capture_From_Environment();
	if (!s_updateActive) {
		Reset_Sample();
	}
	s_sample.renderFrame = renderFrame;
	s_sample.syncTimeMs = syncTimeMs;
	s_sample.rendered = 1;
	s_renderStartUs = Get_Time_Microseconds();
	s_renderActive = true;
}

unsigned long End_Render_Frame(const RenderFrameCounters &counters)
{
	if (!s_renderActive) {
		return 0;
	}

	s_sample.renderCpuUs = Elapsed_Microseconds(s_renderStartUs, Get_Time_Microseconds());
	s_sample.render = counters;
	s_renderActive = false;

	if (!s_updateActive) {
		Publish_Sample();
	}
	return s_sample.renderCpuUs;
}
}
