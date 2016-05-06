/*
 * Copyright (C) Brno University of Technology (BUT)
 *
 * This file is part of software developed by Robo@FIT group.
 *
 * Author: Martin Velas (ivelas@fit.vutbr.cz)
 * Supervised by: Michal Spanel & Adam Herout ({spanel|herout}@fit.vutbr.cz)
 * Date: 10/10/2014
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pcl/common/eigen.h>
#include <pcl/common/transforms.h>
#include <pcl/visualization/cloud_viewer.h>

#include <cv.h>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <but_velodyne/Visualizer2D.h>

using namespace pcl;
using namespace cv;
using namespace std;
using namespace velodyne_pointcloud;

namespace but_velodyne {

Visualizer2D::Visualizer2D(
    const cv::Mat &source_image,
    const cv::Mat &target_image,
    std::string description_) :
        imageFrame(0, 0, source_image.cols, source_image.rows),
        description(description_),
        rng(theRNG()),
        drawingImage(source_image.rows, source_image.cols+target_image.cols, CV_8UC3),
        senzor_position(INT_MIN, INT_MIN) {

  drawingImageLeftHalf  = drawingImage.colRange(0, source_image.cols);
  drawingImageRightHalf = drawingImage.colRange(source_image.cols,
                                                source_image.cols+target_image.cols);
  Mat source_color_image, target_color_image;
  cvtColor(source_image, source_color_image, CV_GRAY2RGB);
  cvtColor(target_image, target_color_image, CV_GRAY2RGB);

  source_color_image.copyTo(drawingImageLeftHalf);
  target_color_image.copyTo(drawingImageRightHalf);
}

Visualizer2D::Visualizer2D(
    const cv::Mat &background, std::string description_) :
        imageFrame(0, 0, background.cols, background.rows),
        description(description_),
        rng(theRNG()),
        drawingImage(background.rows, background.cols, CV_8UC3),
        senzor_position(INT_MIN, INT_MIN) {

  drawingImageLeftHalf  = drawingImage.colRange(0, drawingImage.cols/2);
  drawingImageRightHalf = drawingImage.colRange(drawingImage.cols/2, drawingImage.cols);
  Mat background_color_image, target_color_image;
  cvtColor(background, background_color_image, CV_GRAY2RGB);

  background_color_image.copyTo(drawingImage);
}

Visualizer2D::Visualizer2D(
    cv::Rect image_frame, std::string description_) :
        imageFrame(image_frame),
        description(description_),
        rng(theRNG()),
        drawingImage(image_frame.height, image_frame.width, CV_8UC3, CV_RGB(255, 255, 255)),
        senzor_position(INT_MIN, INT_MIN) {

  drawingImageLeftHalf  = drawingImage.colRange(0, drawingImage.cols/2);
  drawingImageRightHalf = drawingImage.colRange(drawingImage.cols/2, drawingImage.cols);
}


Visualizer2D& Visualizer2D::addLineCorrespondences(
                                           const vector<ImageLine> &source_lines,
                                           const vector<ImageLine> &target_lines,
                                           const vector<DMatch> &matches) {
  for(vector<DMatch>::const_iterator match = matches.begin();
      match < matches.end();
      match++) {
    Scalar color(rng(256), rng(256), rng(256));
    line(drawingImageLeftHalf, source_lines[match->trainIdx].p1, source_lines[match->trainIdx].p2,
         color, 5);
    line(drawingImageRightHalf, target_lines[match->queryIdx].p1, target_lines[match->queryIdx].p2,
         color, 5);

    Point target_start_point = target_lines[match->queryIdx].p1;
    target_start_point.x += drawingImageLeftHalf.cols;
    line(drawingImage, source_lines[match->trainIdx].p1, target_start_point,
         color, 2);
  }

  return *this;
}

Visualizer2D& Visualizer2D::add3DLineCorrenspondences(
                           const vector<PointCloudLine> &source_lines,
                           const vector<PointCloudLine> &target_lines,
                           const vector<DMatch> &matches,
                           const Mat &projection_matrix,
                           const Eigen::Matrix4f &transformation) {
  for(vector<DMatch>::const_iterator match = matches.begin();
        match < matches.end();
        match++) {
    PointCloudLine source_line = source_lines[match->trainIdx];
    PointCloudLine target_line = target_lines[match->queryIdx];

    Scalar color(rng(256), rng(256), rng(256));

    ImageLine source_img_line = source_line.project(projection_matrix, imageFrame);
    ImageLine target_img_line = target_line.project(projection_matrix, imageFrame);

    float t_distance = source_line.distanceTo(target_line.transform(transformation),
                                              PointCloudLine::EUCLIDEAN);
    float r_distance = source_line.distanceTo(target_line.transform(transformation),
                                              PointCloudLine::COSINE_ORIENTATION);

    line(drawingImageLeftHalf,  source_img_line.p1, source_img_line.p2, color, 5);
    line(drawingImageRightHalf, target_img_line.p1, target_img_line.p2, color, 5);

    stringstream ss;
    ss << "t:" << t_distance << ";r:" << r_distance;
    Point text_pt = source_img_line.p1 + Point(0,-10);
    putText(drawingImageLeftHalf, ss.str(), text_pt, FONT_HERSHEY_PLAIN, 1.0, color);

    target_img_line.p1.x += drawingImageLeftHalf.cols;
    line(drawingImage, source_img_line.p1, target_img_line.p1, color, 2);
  }
  cerr << "Viewing " << matches.size() << " matches." << endl;
  return *this;
}

Visualizer2D& Visualizer2D::addHeightMap(const Regular2DGrid<float> &height_map) {
  float min = INFINITY;
  float max = -INFINITY;
  for(int r = 0; r < height_map.rows; r++) {
    for(int c = 0; c < height_map.cols; c++) {
      float val = *height_map.at(r, c);
      if(!isnan(val) && !isinf(val) && val != 0.0) {
        min = MIN(min, val);
        max = MAX(max, val);
      }
    }
  }
  for(int r = 0; r < height_map.rows; r++) {
    for(int c = 0; c < height_map.cols; c++) {
      float val = *height_map.at(r, c);
      if(!isnan(val) && !isinf(val) && val != 0.0) {
        Vec3b &pixel = drawingImage.at<Vec3b>(r, c);
        if(!isnan(val)) {
          Visualizer3D::colorizeIntensity((*height_map.at(r, c) - min) / (max - min),
                                          pixel.val[0], pixel.val[1], pixel.val[2]);
        }
      }
    }
  }
  return *this;
}

Visualizer2D& Visualizer2D::addSenzor(Point pos) {
  senzor_position = pos;
  return *this;
}

void Visualizer2D::show(int wait, float scale) {
  circle(drawingImage, senzor_position, 5, CV_RGB(255, 0, 255), 1);
  Mat drawingImageScaled;
  resize(drawingImage, drawingImageScaled, Size(), scale, scale, INTER_NEAREST);
  imshow(description, drawingImageScaled);
  char key = waitKey(wait);
  imwrite(description + ".png", drawingImageScaled);
}

}
