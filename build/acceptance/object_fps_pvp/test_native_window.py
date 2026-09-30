"""Native collision evidence must use the product's circle, including corners."""
import unittest
from run_native_window import circle_clear_of_aabb


class NativeCircleEvidenceTests(unittest.TestCase):
    def test_recorded_legal_corner_is_outside_circle(self):
        self.assertTrue(circle_clear_of_aabb((11.213167190551758,8.856383323669434),(9,9),(11,11),.25))

    def test_actual_corner_overlap_is_rejected(self):
        self.assertFalse(circle_clear_of_aabb((11.17,8.83),(9,9),(11,11),.25))

    def test_face_overlap_and_inside_wall_are_rejected(self):
        self.assertFalse(circle_clear_of_aabb((11.24,10),(9,9),(11,11),.25))
        self.assertFalse(circle_clear_of_aabb((10,10),(9,9),(11,11),.25))

    def test_tangent_and_clear_face_are_legal(self):
        self.assertTrue(circle_clear_of_aabb((11.25,10),(9,9),(11,11),.25))
        self.assertTrue(circle_clear_of_aabb((11.2501,10),(9,9),(11,11),.25))


if __name__=='__main__':unittest.main()
