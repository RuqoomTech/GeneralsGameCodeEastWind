/*
** Step 03A performance telemetry characterization test.
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

	PerformanceTelemetry::Begin_Render_Frame(42, 1400);
	PerformanceTelemetry::End_Render_Frame(counters);
	PerformanceTelemetry::Stop_Capture();

	if (PerformanceTelemetry::Is_Capturing()) {
		std::remove(path);
		return Fail("capture remained active after Stop_Capture");
	}

	FILE *file = std::fopen(path, "r");
	if (file == 0) {
		return Fail("capture file was not created");
	}

	char header[1024];
	char row[1024];
	if (std::fgets(header, sizeof(header), file) == 0 || std::fgets(row, sizeof(row), file) == 0) {
		std::fclose(file);
		std::remove(path);
		return Fail("capture did not contain header and sample row");
	}
	std::fclose(file);

	const char *expectedHeader =
		"schema_version,capture_index,render_frame,sync_time_ms,render_cpu_us,"
		"draw_calls,triangles,vertices,dx8_triangles,dx8_vertices,skin_draws,"
		"skin_triangles,skin_vertices,sorted_triangles,sorted_vertices,"
		"texture_bytes,texture_count,texture_changes,lightmap_texture_bytes,"
		"lightmap_texture_count,procedural_texture_bytes,procedural_texture_count,"
		"memory_allocations,memory_frees\n";
	if (std::strcmp(header, expectedHeader) != 0) {
		std::remove(path);
		return Fail("CSV schema changed unexpectedly");
	}
	if (Count_Commas(row) != 23) {
		std::remove(path);
		return Fail("sample column count is not stable");
	}
	if (std::strncmp(row, "1,1,42,1400,", 12) != 0) {
		std::remove(path);
		return Fail("sample identity fields are incorrect");
	}
	const char *expectedTail = ",11,65,93,37,61,7,11,13,17,19,23,29,31,43,47,53,59,61,67\n";
	const size_t rowLength = std::strlen(row);
	const size_t tailLength = std::strlen(expectedTail);
	if (rowLength < tailLength || std::strcmp(row + rowLength - tailLength, expectedTail) != 0) {
		std::remove(path);
		return Fail("render counters were not serialized as expected");
	}

	std::remove(path);
	std::puts("Step 03A performance telemetry capture test passed.");
	return 0;
}
