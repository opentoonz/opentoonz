// Standalone geometry regression checks; no GUI or application initialization.
// g++ -std=c++17 -DLINUX -I../../include vectoralignment_test.cpp -o
// /tmp/vectoralignment_test
#include "tools/vectoralignment.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace VectorAlignment;

void expect(double actual, double expected) {
  if (std::abs(actual - expected) < 1e-10) return;
  std::cerr << "Expected " << expected << ", got " << actual << '\n';
  std::exit(1);
}

int main() {
  std::vector<TRectD> boxes = {
      {40, 10, 60, 30}, {0, 0, 10, 10}, {15, 5, 25, 15}};
  auto result = offsets(boxes, ALIGN_LEFT, SELECT_AREA);
  expect(result[0].x, -40);
  expect(result[0].y, 0);
  expect(result[2].x, -15);
  result = offsets(boxes, ALIGN_RIGHT, FIRST_SELECTED);
  expect(result[1].x, 50);
  result = offsets(boxes, ALIGN_TOP, LAST_SELECTED);
  expect(result[0].y, -15);
  result = offsets(boxes, ALIGN_BOTTOM, SELECT_AREA);
  expect(result[0].y, -10);
  result = offsets(boxes, ALIGN_CENTER_H, SELECT_AREA);
  expect(result[0].y, -5);
  expect(result[0].x, 0);
  result = offsets(boxes, ALIGN_CENTER_V, SELECT_AREA);
  expect(result[1].x, 25);
  expect(result[1].y, 0);
  result = offsets(boxes, ALIGN_LEFT, SMALLEST_OBJECT);
  expect(result[0].x, -40);
  result = offsets(boxes, ALIGN_RIGHT, LARGEST_OBJECT);
  expect(result[2].x, 35);
  result = offsets(boxes, DISTRIBUTE_H, SELECT_AREA);
  expect(result[0].x, 0);
  expect(result[1].x, 0);
  expect(result[2].x, 7.5);
  result = offsets(boxes, DISTRIBUTE_V, SELECT_AREA);
  expect(result[2].y, 2.5);
  result = offsets(boxes, DISTRIBUTE_H, FIRST_SELECTED);
  for (const auto &p : result) expect(p.x, 0);
  // Camera distribution places outer edges on the camera bounds and spaces
  // object centers. This remains finite with exactly two objects.
  result = offsets({boxes[0], boxes[1]}, DISTRIBUTE_H, CAMERA_AREA,
                   TRectD(0, 0, 100, 100));
  expect(result[0].x, 40);
  expect(result[1].x, 0);
  result = offsets({boxes[0]}, ALIGN_CENTER_V, CAMERA_AREA,
                   TRectD(-100, -50, 100, 50));
  expect(result[0].x, -50);
  // Coincident centers, empty selections and undersized selections are safe.
  result = offsets(
      {TRectD(0, 0, 10, 10), TRectD(0, 0, 10, 10), TRectD(0, 0, 10, 10)},
      DISTRIBUTE_H, SELECT_AREA);
  for (const auto &p : result) expect(p.x, 0);
  expect(offsets({}, ALIGN_LEFT, SELECT_AREA).size(), 0);
  result = offsets({boxes[0]}, DISTRIBUTE_H, CAMERA_AREA);
  expect(result[0].x, 0);
  // Control points are zero-area boxes; they must participate in the bounds.
  result = offsets({TRectD(20, 30, 20, 30), TRectD(-10, -5, -10, -5)},
                   ALIGN_CENTER_V, SELECT_AREA);
  expect(result[0].x, -15);
  expect(result[1].x, 15);
  result = offsets({TRectD(20, 30, 20, 30), TRectD(-10, -5, -10, -5)},
                   ALIGN_CENTER_H, LAST_SELECTED);
  expect(result[0].y, -35);
  expect(result[1].y, 0);
  std::cout << "Vector alignment geometry checks passed\n";
}
