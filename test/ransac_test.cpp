//
// Created by feixh on 8/29/15.
//

// STL
#include <iostream>
#include <fstream>
#include <vector>

// theia
#include <theia/solvers/ransac.h>
#include <theia/solvers/estimator.h>
#include <theia/solvers/random_sampler.h>
#include <theia/util/timer.h>


// eigen
#include <Eigen/Core>
#include <Eigen/StdVector>

// P3P
#include "pnpsolvers/P3P_Kneip.h"

using namespace theia;
using namespace std;
using namespace Eigen;

struct Match2D3D
{
    Vector3d featureVector;  // unitary bearing vectors
    Vector3d worldPoint;  // points in world coordinate system
};


class P3PEstimator : public Estimator< Match2D3D, Matrix<double, 3, 4 > > {
public:
    P3PEstimator():
        Estimator< Match2D3D, Matrix<double, 3, 4> >(),
        solver(){}

// Get the minimum number of samples needed to generate a model.
    virtual double SampleSize() const {
        return 3;
    }

    // Given a set of data points, estimate the model. Users should implement this
    // function appropriately for the task being solved. Returns true for
    // successful model estimation (and outputs model), false for failed
    // estimation. Typically, this is a minimal set, but it is not required to be.
    virtual bool EstimateModel(const std::vector<Datum> &data, std::vector<Model> *model) const {
        assert(data.size() >= 3);
        Matrix3d featureVectors;
        Matrix3d worldPoints;
        for (size_t i = 0; i < 3; ++i) {
            featureVectors.col(i) = data[i].featureVector;
            worldPoints.col(i)    = data[i].worldPoint;
        }
        int success = solver.computePoses(featureVectors, worldPoints, *model);
        for (auto it = model->begin(); it != model->end();) {
            if ( !it->allFinite() ) {
                it = model->erase(it);
            } else {
                ++it;
            }
        }
        if ( model->empty() ){
            success = -1;
        }
        if ( success == -1 ){
            return false;
        }else{
            return true;
        }
    }
  // Given a model and a data point, calculate the error. Users should implement
  // this function appropriately for the task being solved.
  virtual double Error(const Datum& data, const Model& model) const {
      // model is gwc
      const Vector3d &worldPoint( data.worldPoint );
      Vector3d proj( model.block<3,3>(0,0).transpose()*( worldPoint - model.block<3,1>(0,3) ) );
      if ( proj(2) < 0 ){
          return 1000000;
      }
      const Vector3d &featureVector( data.featureVector );
      double dx( featureVector(0)/featureVector(2) - proj(0)/proj(2) );
      double dy( featureVector(1)/featureVector(2) - proj(1)/proj(2) );
      return dx*dx + dy*dy;
  }
private:
    P3P_Kneip solver;
};

int main()
{
    // load data
    ifstream ifs("../test/data.txt", ifstream::in );
    assert( ifs.is_open() );
    vector< Vector3d > pc;
    vector< Vector3d > pts;
    int n, n_inliers;
    ifs >> n >> n_inliers;
    for ( int i = 0; i < n; ++i )
    {
        float x, y;
        ifs >> x >> y;
        pc.emplace_back( x, y, 1.0 );
        pc.back().normalize();
    }
    for ( int i = 0; i < n; ++i )
    {
        float x, y, z;
        ifs >> x >> y >> z;
        pts.emplace_back(x, y, z);
    }
    float gwc[3][4];
    for ( int i = 0; i < 3; ++i ) {
        for (int j = 0; j < 4; ++j) {
            ifs >> gwc[i][j];
        }
    }
    ifs.close();
    vector< Match2D3D > data( n );
    for ( size_t i = 0; i < n; ++i )
    {
        data[i].featureVector = pc[i];
        data[i].worldPoint    = pts[i];
    }
    cout << "got " << n << " matches" << endl;

    // setup ransac parameters
    RansacParameters ransac_params;
    ransac_params.error_thresh = 1e-2;
    ransac_params.failure_probability = 0.05;
    ransac_params.max_iterations = 10000;
    ransac_params.min_inlier_ratio = 0.2;
    ransac_params.use_mle = false;


    P3PEstimator estimator;
    Ransac< P3PEstimator > ransac(ransac_params, estimator);
    ransac.Initialize();
    RansacSummary summary;
    Matrix< double, 3, 4 > best_model;
    Timer tt;
    ransac.Estimate( data, &best_model, &summary );
    double duration( tt.ElapsedTimeInSeconds() );
    cout << duration << " s" << endl;

    // checkout results
    cout << best_model << endl;
    cout << "iterations:" << summary.num_iterations << endl;
    for ( size_t i = 0; i < summary.inliers.size(); ++i )
    {
        cout << summary.inliers[i] << " ";
    }
    cout << endl;
}
