///
/// \file DenseGrid.h
///
/// \brief Public API for the Dense Grid data structure
///
/// \author Kacey Coley
/// \date June 19, 2014
/// \since 0.4
///
/// DenseGrid is a data structure which allocates a set grid resolution.
/// As opposed to a sparse grid, the dimension of a dense grid is rigidly defined


#ifndef __DENSEGRID_H__
#define __DENSEGRID_H__
#include "MPUtils/Vector.h"
#include "MPVolume/MPVolumeGrid.h"
#include <openvdb/Grid.h>
#include <openvdb/tools/Interpolation.h>
#include <openvdb/openvdb.h>
#include <openvdb/math/Transform.h>
#include <openvdb/math/BBox.h>
#include <cmath>


using MeshPotato::MPUtils::MPVec3;
/// Partial Template Instantiation for Grid Types

namespace MeshPotato {
  namespace MPVolume {

    template<typename T>
    struct MPGridType {
      typedef openvdb::FloatGrid VDBGridT;
    };
    template<>
    struct MPGridType<float> {
      typedef openvdb::FloatGrid VDBGridT;
    };
    template<>
    struct MPGridType<MPUtils::Color> {
      typedef openvdb::VectorGrid VDBGridT;
    };
    template<>
    struct MPGridType<MPUtils::MPVec3> {
      typedef openvdb::VectorGrid VDBGridT;
    };

    template <typename T>
    class DenseGrid: public MeshPotato::MPVolume::VolumeGrid<T> {
    public:
      typedef typename MPGridType<T>::VDBGridT MPGridT;
      typedef typename MPGridT::Accessor AccessorT;
      typedef typename MPGridT::Ptr MPGridPtr;
      typedef typename openvdb::tools::GridSampler<MPGridT, openvdb::tools::BoxSampler> SamplerT;

      static boost::shared_ptr<VolumeGrid<T> > Ptr(const MPUtils::BBox _box,
                                                   const float _resolution
                                                  );

      DenseGrid(const MPUtils::BBox _bbox,
      const float _resolution);

      /*! \brief evaluates the grid at a particular world space point using trilinear
      *           interpolation
      *
      * \param MPVec3 &P is a const vector representing a world space position
      * \return const typename Volume<T>::volumeDataType
      */
      const typename Volume<T>::volumeDataType  eval(const MPVec3 &P) const;
      const typename Volume<T>::volumeGradType  grad(const MPVec3 &P) const;
      void set(const MPUtils::Coord ijk, const T value);
      const T get(const MPUtils::Coord ijk) const;
      void add(const MPUtils::Coord ijk, const T value);
      virtual void stamp(boost::shared_ptr<Volume<T> > volume);
      const MPUtils::MPVec3 indexToWorld(const MPUtils::Coord ijk) const;
      const MPUtils::MPVec3 worldToIndex(const MPUtils::MPVec3 P) const;
      const int nx() const;
      const int ny() const;
      const int nz() const;

      boost::shared_ptr<VolumeGrid<T> > deepCopy();
    private:
      MPUtils::BBox bbox;
      float voxelSize;
      MPGridPtr grid;
      SamplerT interpolator;
      AccessorT accessor;
      MPUtils::Coord dim;
    };

    typedef DenseGrid<float> DenseGridFloat;
    typedef DenseGrid<MPUtils::MPVec3> DenseGridVector;
    typedef boost::shared_ptr<MeshPotato::MPVolume::VolumeGrid<float> > VolumeGridFloatPtr;
    typedef boost::shared_ptr<MeshPotato::MPVolume::VolumeGrid<MPUtils::MPVec3> > VolumeGridVectorPtr;
    typedef VolumeGrid<float> VolumeGridFloat;
    typedef VolumeGrid<MPUtils::MPVec3> VolumeGridVector;
  }

}
#endif // __DENSEGRID_H__
