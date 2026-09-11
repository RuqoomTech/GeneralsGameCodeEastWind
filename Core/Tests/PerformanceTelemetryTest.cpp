/*
** Step 03 performance telemetry characterization test.
*/

#include "rts/profile.h"

#include <cstdio>
#include <cstring>

namespace
{
int Fail(const char *message)
{
	std::fprintf(stderr, "Performance telemetry test failed: %s\n", message);
	return 1;
}

int Count_Commas(const char *text)
{
	int count = 0;
	for (; *text != '\0'; ++text) {
		if (*text == ',') {
			++count;
		}
	}
	return count;
}
}

int main()
{
	const char *path = "performance-telemetry-test.csv";
	std::remove(path);

	if (PerformanceTelemetry::Start_Capture("") || PerformanceTelemetry::Is_Capturing()) {
		return Fail("empty capture paths must stay disabled");
	}
	if (!PerformanceTelemetry::Start_Capture(path) || !PerformanceTelemetry::Is_Capturing()) {
		return Fail("explicit capture could not be started");
	}

	PerformanceTelemetry::RenderFrameCounters counters;
	counters.drawCalls = 11;
	counters.dx8Triangles = 37;
	counters.dx8Vertices = 61;
	counters.skinDraws = 7;
	counters.skinTriangles = 11;
	counters.skinVertices = 13;
	counters.sortedTriangles = 17;
	counters.sortedVertices = 19;
	counters.textureBytes = 23;
	counters.textureCount = 29;
	counters.textureChanges = 31;
	counters.lightmapTextureBytes = 43;
	counters.lightmapTextureCount = 47;
	counters.proceduralTextureBytes = 53;
	counters.proceduralTextureCount = 59;
	counters.memoryAllocations = 61;
	counters.memoryFrees = 67;

	PerformanceTelemetry::Begin_Update_Frame(77);
	PerformanceTelemetry::Begin_Update_Phase(PerformanceTelemetry::UPDATE_PHASE_CLIENT);
	PerformanceTelemetry::Record_Drawable_Visibility(10, 6, 3);
	PerformanceTelemetry::Begin_Render_Frame(42, 1400);
	PerformanceTelemetry::End_Render_Frame(counters);
	PerformanceTelemetry::End_Update_Phase(PerformanceTelemetry::UPDATE_PHASE_CLIENT);
	PerformanceTelemetry::Begin_Update_Phase(PerformanceTelemetry::UPDATE_PHASE_LOGIC);
	PerformanceTelemetry::End_Update_Phase(PerformanceTelemetry::UPDATE_PHASE_LOGIC);
	PerformanceTelemetry::End_Update_Frame();
	PerformanceTelemetry::Stop_Capture();

	if (PerformanceTelemetry::Is_Capturing()) {
		std::remove(path);
		return Fail("capture remained active after Stop_Capture");
	}

	FILE *file = std::fopen(path, "r");
	if (file == 0) {
		return Fail("capture file was not created");
	}

	char header[2048];
	char row[2048];
	if (std::fgets(header, sizeof(header), file) == 0 || std::fgets(row, sizeof(row), file) == 0) {
		std::fclose(file);
		std::remove(path);
		return Fail("capture did not contain header and sample row");
	}
	std::fclose(file);

	const char *expectedHeader =
		"schema_version,capture_index,render_frame,logic_frame,sync_time_ms,rendered,logic_updated,"
		"update_cpu_us,client_cpu_us,logic_cpu_us,network_cpu_us,message_cpu_us,render_cpu_us,"
		"drawable_total,drawable_visible,drawable_shrouded,"
		"draw_calls,triangles,vertices,dx8_triangles,dx8_vertices,skin_draws,"
		"skin_triangles,skin_vertices,sorted_triangles,sorted_vertices,"
		"texture_bytes,texture_count,texture_changes,lightmap_texture_bytes,"
		"lightmap_texture_count,procedural_texture_bytes,procedural_texture_count,"
		"memory_allocations,memory_frees\n";
	if (std::strcmp(header, expectedHeader) != 0) {
		std::remove(path);
		return Fail("CSV schema changed unexpectedly");
	}
	if (Count_Commas(row) != 34) {
		std::remove(path);
		return Fail("sample column count is not stable");
	}
	if (std::strncmp(row, "2,1,42,77,1400,1,1,", std::strlen("2,1,42,77,1400,1,1,")) != 0) {
		std::remove(path);
		return Fail("sample identity/update-state fields are incorrect");
	}
	const char *expectedCounters = ",10,6,3,11,65,93,37,61,7,11,13,17,19,23,29,31,43,47,53,59,61,67\n";
	const size_t rowLength = std::strlen(row);
	const size_t countersLength = std::strlen(expectedCounters);
	if (rowLength < countersLength || std::strcmp(row + rowLength - countersLength, expectedCounters) != 0) {
		std::remove(path);
		return Fail("visibility/render counters were not serialized as expected");
	}

	std::remove(path);
	std::puts("Step 03 performance telemetry capture test passed.");
	return 0;
}
