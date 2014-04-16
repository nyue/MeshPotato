#ifndef __RAYMARCHER_H__
#define __RAYMARCHER_H__
#include "ImplicitShape.h"
#include <iostream>
#include <openvdb/Grid.h>
#include <openvdb/tools/Interpolation.h>
#include <openvdb/tools/RayIntersector.h>
#include <MPVolume/FrustumGrid.h>
#include "MPUtils/DeepImage.h"
#include "MPUtils/Image.h"
#include "tbb/tbb.h"
#include "MPUtils/OIIOFiles.h"
using namespace MeshPotato::MPUtils;
namespace MeshPotato {
namespace MPVolume {

class VDBRayMarcher {
public:
typedef openvdb::tools::VolumeRayIntersector<openvdb::FloatGrid> IntersectorT;
typedef openvdb::FloatGrid::ConstAccessor  AccessorType;
typedef openvdb::tools::BoxSampler SamplerT;
typedef openvdb::tools::GridSampler<AccessorType, SamplerT> SamplerType;
VDBRayMarcher(openvdb::FloatGrid::Ptr _grid, 
	     VolumeColorPtr _dsm, 
	     const double &_step, 
	     const double &_K, 
	     boost::shared_ptr<MeshPotato::MPUtils::Image> _image, 
	     boost::shared_ptr<MeshPotato::MPUtils::DeepImage> _deepimage, 
	     boost::shared_ptr<MeshPotato::MPUtils::Camera> _camera, 
	     const std::string &_outputImage
	     ) : 
	     grid(_grid), 
	     dsm(_dsm), 
	     step(_step), 
	     K(_K), 
	     interpolator(grid->constTree(), grid->transform()), 
	     intersector(*grid), 
	     image(_image), 
	     deepimage(_deepimage), 
	     camera(_camera), 
	     outputImage(_outputImage),
       	     accessor(grid->getConstAccessor())	{}

//const MeshPotato::MPUtils::Color L(MPRay &ray, openvdb::tools::VolumeRayIntersector<openvdb::FloatGrid> &intersector2) const {
//const inline MeshPotato::MPUtils::Color L(MPRay &ray, IntersectorT &intersector2, openvdb::tools::GridSampler<openvdb::FloatTree, openvdb::tools::BoxSampler> &interpolator2) const {
const inline MeshPotato::MPUtils::Color L(MPRay &ray, IntersectorT &intersector2, SamplerType &interpolator2) const {
	Color _L = Color(0,0,0,0);
	float deltaT, deltaS;
	float _T = 1.0f;
	float time = 0.0f;	
	float steptime = 0.0f;	
//	openvdb::tools::GridSampler<openvdb::FloatTree, openvdb::tools::BoxSampler> interpolator2(grid->constTree(), grid->transform());
	if (!intersector2.setWorldRay(ray)) return _L;
	double t0 = 0, t1 = 0;
	while (int n = intersector2.march(t0, t1)) {
//		for (double time = step*ceil(t0/step); time <= t1; time += step) {
		for (double time = step*ceil(t0/step); time <= t1; time += step) {
				MPVec3 P = intersector2.getWorldPos(time);
				double density = interpolator2.wsSample(P);

				if (density > 0) {
					deltaT = exp(-K*density*step);
					Color CM(1,1,1,1);
			                Color CI = dsm->eval(P);
			                Color CS = CI*CM;
	            
					_L += ((CS)*_T*(1.0 - deltaT));
					_T *=deltaT;
					if (_T < 0.0000001) return Color(_L[0],_L[1],_L[2],1.0 - _T);
				}
		}

	}
	return Color(_L[0],_L[1],_L[2],1.0 - _T);
}
const MeshPotato::MPUtils::DeepPixelBuffer deepL(MPRay &ray, IntersectorT &intersector2, SamplerType &interpolator2) const {
//const MeshPotato::MPUtils::DeepPixelBuffer deepL(MPRay &ray, openvdb::tools::VolumeRayIntersector<openvdb::FloatGrid> &intersector2) const {
	float deltaT, deltaS;
	float steptime = 0.0f;	
	MeshPotato::MPUtils::DeepPixelBuffer deepPixelBuf;

	if (!intersector2.setWorldRay(ray)) return deepPixelBuf;
	double t0 = 0, t1 = 0;
	while (int n = intersector2.march(t0, t1)) {
		for (float time = step*ceil(t0/step); time <= t1; time += step) {
				MPVec3 P = intersector2.getWorldPos(time);
				double density = interpolator2.wsSample(P);

				if (density > 0) {
					deltaT = exp(-K*density*step);
					Color CM(1,1,1,1);
	                		Color CI = dsm->eval(P);
					Color CS = CI*CM;

					MeshPotato::MPUtils::DeepPixel deepPixel;

	            
					Color _L = ((CS)*(1 - deltaT));
					deepPixel.color = _L;
					deepPixel.color[3] = 1.0 - deltaT;
					deepPixel.depth_front = (P - camera->eye()).length();//(ray(time)).dot(cam.view());
					deepPixelBuf.push_back(deepPixel);
				}
		}

	}
	return deepPixelBuf;
}
void operator() (const tbb::blocked_range<size_t>& r) const {
	for (size_t j = r.begin(), je = r.end(); j < je; ++j) {
	AccessorType accessor2(grid->getConstAccessor());
	SamplerType interpolator2(accessor2, grid->transform());
		// Create an intersector for each thread
		IntersectorT intersector2(*grid);
		for (size_t i = 0, ie = image->Width(); i < ie; ++i) {
			MeshPotato::MPUtils::MPRay ray;
			double x = (double)i/(image->Width() - 1.0);
               		double y = (double)j/(image->Height() - 1.0);
			ray = camera->getRay(x,y);

			const DeepPixelBuffer deepColor = deepL(ray, intersector2, interpolator2);
//			std::vector<float> &pixel = image->pixel(i,j);
//			pixel[0] = c[0];
//			pixel[1] = c[1];
//			pixel[2] = c[2];
//			pixel[3] = c[3];
			deepimage->value(i,j) = deepColor;
		}
	}
}
inline void render(bool threaded) const {
	tbb::blocked_range<size_t> range(0, image->Height());
	threaded ? tbb::parallel_for(range, *this) : (*this)(range);
}
void writeImage() {
	boost::shared_ptr<Image> tempimage = deepimage->flatten();
	MeshPotato::MPUtils::writeOIIOImage(outputImage.c_str(), *tempimage);
//	MeshPotato::MPUtils::writeOIIOImage(outputImage.c_str(), *image);
	 MeshPotato::MPUtils::writeOIIOImage(("deep"+outputImage).c_str(), *deepimage);
}
private:
openvdb::FloatGrid::Ptr grid;
VolumeColorPtr dsm;
double K, step;
openvdb::tools::GridSampler<openvdb::FloatTree, openvdb::tools::BoxSampler> interpolator;
openvdb::tools::VolumeRayIntersector<openvdb::FloatGrid> intersector;
boost::shared_ptr<MeshPotato::MPUtils::Image> image;
boost::shared_ptr<MeshPotato::MPUtils::DeepImage> deepimage;
boost::shared_ptr<MeshPotato::MPUtils::Camera> camera;
std::string outputImage;
AccessorType accessor;
};

class RayMarcher {
public:
RayMarcher(VolumeFloatPtr _grid, VolumeColorPtr _dsm, const float &_step, const float &_K, boost::shared_ptr<Camera> _camera, const MPUtils::BBox _bbox, boost::shared_ptr<MPUtils::Image> _image) : grid(_grid), dsm(_dsm), step(_step), K(_K), camera(_camera), bbox(_bbox), image(_image)  {}
MeshPotato::MPUtils::Color L(MPRay &ray) const {
        Color _L = Color(0,0,0,0);
        float deltaT;
        float _T = 1.0f;
        float time = 0.0f;
        double t0 = ray.t0(), t1 = ray.t1();
	time = t0;
        while (time < t1) {
                        time += step;
                        while (time < t1) {
                        MeshPotato::MPUtils::MPVec3 P = ray(time);
                        float density = grid->eval(P);
                        if (density > 0) {
                                deltaT = exp(-K * density * step);
				Color CI = dsm->eval(P); 
				Color CM(1,1,1,1);
				Color CS = CI*CM;
//                              Color CS = density * (Color(1.0, 1.0, 1.0, 1.0) * exp(-dsm->eval(P) * K));
                                _L += ((CS) * _T * (1.0 - deltaT));
                                _T *=deltaT;
                                if (_T < 0.0000001) break;
                        }
                        time += step;
                	}
        }
        return Color(_L[0],_L[1],_L[2],1.0 - _T);
}
void operator() (const tbb::blocked_range<size_t>& r) const {
	for (size_t j = r.begin(), je = r.end(); j < je; ++j) {
		for (size_t i = 0, ie = image->Width(); i < ie; ++i) {
			MeshPotato::MPUtils::MPRay ray;
			double x = (double)i/(image->Width() - 1.0);
               		double y = (double)j/(image->Height() - 1.0);
			ray = camera->getRay(x,y);

//			const DeepPixelBuffer deepColor = deepL(ray, intersector2, interpolator2);
			const Color c = L(ray);
			std::vector<float> &pixel = image->pixel(i,j);
			pixel[0] = c[0];
			pixel[1] = c[1];
			pixel[2] = c[2];
			pixel[3] = c[3];
//			deepimage->value(i,j) = deepColor;
		}
	}
}
inline void render(bool threaded) const {
        tbb::blocked_range<size_t> range(0, image->Height());
        threaded ? tbb::parallel_for(range, *this) : (*this)(range);
}
void writeImage(const std::string outputImage) {
//        Image tempimage = deepimage->flatten();
  //      MeshPotato::MPUtils::writeOIIOImage(outputImage.c_str(), tempimage);
      MeshPotato::MPUtils::writeOIIOImage(outputImage.c_str(), *image);
  //       MeshPotato::MPUtils::writeOIIOImage(("deep"+outputImage).c_str(), *deepimage);
}

private:
MeshPotato::MPVolume::VolumeFloatPtr grid;
VolumeColorPtr dsm;
float K, step;
boost::shared_ptr<Camera> camera;
MPUtils::BBox bbox;
boost::shared_ptr<MPUtils::Image> image;
};




}
}
#endif // __RAYMARCHER_H__
