#include "Track.h"
#include <Eigen/Geometry>




Track::Track(int index, pcl::PointXYZRGB initTranslation, int instanceID, double matchDThresh,int sepThresh, int resFracMultiplier)
{
    resolutionFracMultiplier = resFracMultiplier;
    frameIndex = index;
    birthFrameIndex = index;
    deathFrameIndex = 0;
    IDnum = instanceID;
    initialTranslation = initTranslation;
    matchDistanceThreshold = matchDThresh;
    nukeDistanceThreshold = sepThresh;
    numberOfContinuousZombieFrames = 0;
    isBirthable = false;
    isBirthFrame=true;

    //initialize in case of no matching to models
    QString modelType="none";

}

Track::~Track()
{
}

/**
 * Given a vector of data points and a vector of model
 * points, use ICP to register the model to the data.  Add registration transformation
 * to transforms vector and registration transformation from
 * the track's birth frame to absoluteTransforms vector.
 */
pcl::PointCloud<pcl::PointXYZRGB> Track::update(pcl::PointCloud<pcl::PointXYZRGB> dataPTS_cloud,vector<Model> modelPTS_clouds,
                                             int modelTODataThreshold, int separateThreshold, int matchDThresh,
                                             int ICP_ITERATIONS, double ICP_TRANSEPSILON, double ICP_EUCLIDEANDIST)
{

    matchDistanceThreshold=matchDThresh;
    nukeDistanceThreshold = separateThreshold;

    icp_maxIter=ICP_ITERATIONS;
    icp_transformationEpsilon=ICP_TRANSEPSILON;
    icp_euclideanDistance=ICP_EUCLIDEANDIST;


    pcl::PointCloud<pcl::PointXYZRGB> modelToProcess_cloud;

    Eigen::Matrix4f T;

    T.setIdentity();


    double birthAssociationsThreshold = modelPTS_clouds[0].cloud.size() * .01 *modelTODataThreshold; //TODO Shouldn't hardcode this to the first!

    double healthyAssociationsThreshold = birthAssociationsThreshold*.4; //Still healthy with 40 percent of a whole model


    if (frameIndex == birthFrameIndex)
    {

        isBirthFrame=true; // Try out Doing extra work aligning the objects if it is the very first frame
        //Determine what model this creature should use
        modelIndex= identify(dataPTS_cloud,modelPTS_clouds);


    }
    else
    {
        isBirthFrame=false;


    }







    modelToProcess_cloud = modelPTS_clouds[modelIndex].cloud;

    //leave open for other possible ICP methods
    bool doPCL=true;
    //PCL implementation of ICP
    if(doPCL){

        if (dataPTS_cloud.size() > 1) //TODO FIX the math will throw an error if there are not enough data points
        {

            //STRIP 3D data
            PointCloud<PointXY> dataPTS_cloud2D;
            copyPointCloud(dataPTS_cloud,dataPTS_cloud2D);
            PointCloud<PointXY> modelPTS_cloud2D;
            copyPointCloud(modelToProcess_cloud,modelPTS_cloud2D);

                     PointCloud<PointXYZRGB> dataPTS_cloudStripped;
                     copyPointCloud(dataPTS_cloud2D,dataPTS_cloudStripped);

                     PointCloud<PointXYZRGB> modelPTS_cloudStripped;
                     copyPointCloud(modelPTS_cloud2D,modelPTS_cloudStripped);



            // Find transformation from the orgin that will optimize a match between model and target
            T=   updateTransformPCLRGB(dataPTS_cloudStripped, modelPTS_cloudStripped);


            pcl::transformPointCloud(modelToProcess_cloud,modelToProcess_cloud, T);

        }

    }

    else{ //no other ICP implementation currently
    }


    int totalpointsBeforeRemoval=dataPTS_cloud.size();
    int totalRemovedPoints = 0;

    /** Remove old data points associated with the model
      * delete the points under where the model was placed. The amount of points destroyed in this process gives us a metric for how healthy the track is.
      *
      **/
    pcl::PointCloud<pcl::PointXYZRGB> dataPTSreduced_cloud;

    removeClosestDataCloudPoints(dataPTS_cloud, modelToProcess_cloud, nukeDistanceThreshold );
    pcl::copyPointCloud(removeClosestDataCloudPoints(dataPTS_cloud, modelToProcess_cloud, nukeDistanceThreshold ),dataPTSreduced_cloud);

    totalRemovedPoints = totalpointsBeforeRemoval - dataPTSreduced_cloud.size();




    //////
    ///Determine Health of latest part of track
    ////////

    // Just born tracks go here: //Should get rid of this, doesn't make sense
    if (frameIndex == birthFrameIndex)
    {

        absoluteTransforms.push_back(T);

        //Check number of data points associated with this track, if it is greater than
        // the birth threshold then set birth flag to true.
        //If it doesn't hit this, it never gets born ever

        if ( totalRemovedPoints>birthAssociationsThreshold ) //matchScore >birthAssociationsThreshold && //Need new check for birthAssociationsthresh//  closestToModel.size() >= birthAssociationsThreshold)
        {
            isBirthable=true;



         /**  /// For Debugging we can visualize the Pointcloud
            pcl:: PointCloud<PointXYZRGB>::Ptr dataPTS_cloud_ptr (new pcl::PointCloud<PointXYZRGB> (dataPTS_cloud));
          //  copyPointCloud(modelPTS_cloud,)
            transformPointCloud(modelPTS_clouds[modelIndex].first,modelPTS_clouds[modelIndex].first,T);

            pcl:: PointCloud<PointXYZRGB>::Ptr model_cloud_ptrTempTrans (new pcl::PointCloud<PointXYZRGB> (modelPTS_clouds[modelIndex].first));

            pcl::visualization::CloudViewer viewer("Simple Cloud Viewer");
            viewer.showCloud(dataPTS_cloud_ptr);

        int sw=0;
                    while (!viewer.wasStopped())
                    {
                        if(sw==0){
                        viewer.showCloud(model_cloud_ptrTempTrans);
                        sw=1;
                        }
                        else{
                            viewer.showCloud(dataPTS_cloud_ptr);
        sw=0;
                        }

                    }
        /**/

        }

    }
    // Tracks that are older than 1 frame get determined here
    else
    {
        //Check the number of data points associated with this track, if it is less than
        //the zombie/death threshold, then copy previous transform, and add this frame
        //index to zombieFrames

        //Is the track unhealthy? if so make it a zombie
        if ( totalRemovedPoints<healthyAssociationsThreshold )// !didConverge)// No zombies for now! eternal life! !didConverge) //closestToModel.size() < healthyAssociationsThreshold && absoluteTransforms.size() > 2)
        {

            absoluteTransforms.push_back(T);

            zombieIndicesIntoAbsTransforms.push_back(absoluteTransforms.size()-1);
            numberOfContinuousZombieFrames++;
        }
        else // Not zombie frame, keep using new transforms
        {

            // add to transforms
            absoluteTransforms.push_back(T);
            numberOfContinuousZombieFrames = 0;

        } //not zombie frame
    }// Not first frame



    // increment frame counter
    frameIndex++;

    return dataPTSreduced_cloud;

}

/**
  Loop over container of pairs of model clouds and string identifiers

  transform each

  find the one with the best fit

  choose this one as the identity


  **/

int Track::identify (PointCloud<PointXYZRGB> dataPTS_cloud,vector<Model> modelgroup){

    float fit=DBL_MAX;
    int identity=0;
    for (int i=0; i<modelgroup.size();i++){

        qDebug()<<"Model Cloud "<<modelgroup[i].name<<"  idx "<<i;

//        vector< pair<PointCloud<pcl::PointXYZRGB>, QString> >
        PointCloud<PointXYZRGB> modelTOIdentify = modelgroup[i].cloud;
                 updateTransformPCLRGB(dataPTS_cloud, modelTOIdentify);
                 if(recentFitness<fit){
                     fit = recentFitness;
                  identity = i;
                 }


    }
    qDebug()<<"end ID testing, selected value was: "+modelgroup[identity].name;

    return identity;

}


/**
 * Apply transformation to an input model_Cloud
 */
void Track::transformCloud(pcl::PointCloud<pcl::PointXYZRGB> modelPTS_cloud, Eigen::Matrix4f transform)
{
    pcl::transformPointCloud(modelPTS_cloud,modelPTS_cloud, transform); //This function call is weird! need to dereference pointers

}






Eigen::Matrix4f Track::updateTransformPCLRGB(pcl::PointCloud<pcl::PointXYZRGB> data_cloud,pcl::PointCloud<pcl::PointXYZRGB> model_cloud){

    Eigen::Matrix4f ET;
    Matrix4f guess,guess180;
    guess.setIdentity();
    guess180.setIdentity();
    pcl:: PointCloud<PointXYZRGB>::Ptr model_cloud_ptr (new pcl::PointCloud<PointXYZRGB> (model_cloud));
    pcl:: PointCloud<PointXYZRGB>::Ptr data_cloud_ptr (new pcl::PointCloud<PointXYZRGB> (data_cloud));

    //Setup ICP
    pcl::IterativeClosestPoint<pcl::PointXYZRGB, pcl::PointXYZRGB> icp;
    icp.setInputCloud(model_cloud_ptr);
    icp.setInputTarget(data_cloud_ptr);
    pcl::PointCloud<pcl::PointXYZRGB> Final;








    // if(model_cloud_ptr->is_dense){ //TODO Do we need this?

    //////////
    ////Set Parameters

    //TODO Test different Parameters for First frame and all others
    //First frame should match REALLY WELL

    //However, we also just removed noisy data this way, and so we would be spending lots of extra time
    //trying to align against a cluster of points we really just want to remove

    if(isBirthFrame){

        // Set the max correspondence distance (e.g., correspondences with higher distances will be ignored)

//              icp.setMaxCorrespondenceDistance (DBL_MAX);
              icp.setMaxCorrespondenceDistance (matchDistanceThreshold);

        // Set the maximum number of iterations (criterion 1)
        icp.setMaximumIterations (icp_maxIter*3);  //Timeout

        //Set the transformation epsilon (criterion 2)
        //This is the maximum distance that the model can move between iterations and still be thought to have converged
        icp.setTransformationEpsilon (icp_transformationEpsilon);

        //Set the euclidean distance difference epsilon (criterion 3)
        //This is how well a model needs to fit to consider the model to have converged
        // icp.setEuclideanFitnessEpsilon (icp_euclideanDistance);



        //Other Parameters

        //        icp.setRANSACIterations(icp_maxIter); //RANSAC needs the below parameter to work unles it is zero, default RANSAC properties are 1000 iterations and 0.05 distance threshold
        icp.setRANSACIterations(0); //RANSAC needs the below parameter to work unless it is zero, default RANSAC properties are 1000 iterations and 0.05 distance threshold
//        icp.setRANSACOutlierRejectionThreshold(matchDistanceThreshold);


        /// Create Guess For Birthframe
        // This matrix is a Matrix4F composed of (what i think) is a translation vector on the right hand column,
        // and the top left 3X3 matrix is an AngleAxis rotation matrix

        guess <<    1,0,0, initialTranslation.x,
                0,1,0,initialTranslation.y,
                0,0,1,0,
                0,0,0,1;


    }
    else{

        // Set the max correspondence distance (e.g., correspondences with higher distances will be ignored)

        //Currently thinking that there should be no max distance, because with noisy data, erroneous points take chunks out of good detections
              icp.setMaxCorrespondenceDistance (matchDistanceThreshold);
//        icp.setMaxCorrespondenceDistance (DBL_MAX);


        // Set the maximum number of iterations (criterion 1)
        //int maxIt = uitrack.ICP_MaxIterspinBox->value(); //FIX this doesn't work to connect to the UI in this way
        icp.setMaximumIterations (icp_maxIter);  //Timeout



        //Set the transformation epsilon (criterion 2)
        //This is the maximum distance that the model can move between iterations and still be thought to have converged
        icp.setTransformationEpsilon (icp_transformationEpsilon); //Default value is 0

        //Set the euclidean distance difference epsilon (criterion 3)
        //This is how well a model needs to fit to consider the model to have converged
        //the maximum allowed Euclidean error between two consecutive steps in the ICP loop, before
        //          * the algorithm is considered to have converged.
        //          * The error is estimated as the sum of the differences between correspondences in an Euclidean sense,
        //          * divided by the number of correspondences.
        //  icp.setEuclideanFitnessEpsilon (icp_euclideanDistance); //Default value is   -1.79769e+308


        //Other Parameters
        // icp.setRANSACIterations(icp_maxIter); //RANSAC needs the below parameter to work unles it is zero, default RANSAC properties are 1000 iterations and 0.05 distance threshold

        icp.setRANSACIterations(0); //RANSAC needs the below parameter to work unles it is zero, default RANSAC properties are 1000 iterations and 0.05 distance threshold
//        icp.setRANSACOutlierRejectionThreshold(matchDistanceThreshold);

        /// Create Guess For Latterframes
        // use last transformation added
        guess= absoluteTransforms.back();


    }

    //////////////
    ///Align 1
    //// Primary alignement with initial guess

    icp.align(Final,guess);

    ET=icp.getFinalTransformation();

    recentFitness = icp.getFitnessScore();

    qDebug() << "has converged:" << icp.hasConverged() << " score: " <<recentFitness<<"  RANSAC Iterations: " <<icp.getRANSACIterations()<< "RansacOutlierRejection"<< icp.getRANSACOutlierRejectionThreshold() << " Transepsilon: " <<icp.getTransformationEpsilon() <<" EuclideanFitnes: " <<icp.getEuclideanFitnessEpsilon()<< "  Data Points in sight: "<<data_cloud.size();
    qDebug();

    matchScore = icp.getFitnessScore();
    didConverge = icp.hasConverged();




    if(isBirthFrame ){

        //180 flip calculated manually
        //Feed in the results of the previous alignment
        // use the full previous transformation and just flip it 180 about its centroid
        //Let's give it a try
        guess180 <<    -1,0,0,0,
                0,-1,0,0,
                0,0,1,0,
                0,0,0,1;
       guess180=guess*guess180;//Multiply in the reverse order that you would like to perform the transformations

      // guess180= ET*guess180; //For some reason this doesn't work, I thought this would be more optimal? but it misses the flip more often! I must be envisioning the math wrong.The above version even works better, goes 10% faster
        pcl::IterativeClosestPoint<pcl::PointXYZRGB, pcl::PointXYZRGB> icp2;
        icp2=icp;


        icp2.align(Final, guess180);

        if(icp2.getFitnessScore()<recentFitness){
            recentFitness=icp2.getFitnessScore();
        }
        qDebug() << "180 has converged:" << icp2.hasConverged() << " score: " <<icp2.getFitnessScore()<<"  RANSAC Iterations: " <<icp2.getRANSACIterations()<< "RansacOutlierRejection"<< icp2.getRANSACOutlierRejectionThreshold() << " Transepsilon: " <<icp2.getTransformationEpsilon() <<" EuclideanFitnes: " <<icp2.getEuclideanFitnessEpsilon()<< "  Data Points in sight: "<<data_cloud.size();
        qDebug();

        //Temp invert!
        if(icp2.getFitnessScore()<matchScore){
            qDebug()<<"Flip it!" ;
            return icp2.getFinalTransformation();
        }


    }
    return ET;

}


double Track::getX(int idx)
{
    if (idx < 0) idx = frameIndex;
    Eigen::Matrix4f T = absoluteTransforms[idx-birthFrameIndex-1]; //TODO why the -1?
    return T(0,3);
}

double Track::getY(int idx)
{
    if (idx < 0) idx = frameIndex;
    Eigen::Matrix4f T = absoluteTransforms[idx-birthFrameIndex-1];
    return T(1,3);
}

pair <Point,double> Track::getXYT(int idx){
    //cout << " --- Get Rotations --- " << endl;
pair <Point,double> returnInfo;
    if (idx < 0) idx = frameIndex;
    Eigen::Matrix4f T = absoluteTransforms[idx-birthFrameIndex-1];
    float theta;

    //Manually create a cloud and shift
    pcl::PointCloud<pcl::PointXYZ>  standardcloud;




    // make the angle match convention of pointing down X axis = 0 degrees, poiting down positive y axis = 90 degrees

    pcl::PointXYZ p1,p2;
    p1.x=0; p1.y=0;
    p2.x=100; p2.y=0;

    standardcloud.push_back(p1);
    standardcloud.push_back(p2);
    // transformCloud(standardcloud, T);
    pcl::transformPointCloud(standardcloud,standardcloud, T);

    theta = atan2((float)(standardcloud.points[1].y-standardcloud.points[0].y), (float)(standardcloud.points[1].x-standardcloud.points[0].x)); // changed this function to make consistent
    //  cout << " The manually translated angle in degrees : " <<theta*180/3.14157 << endl;

    /* I don't know why this doesn't work for angles between 240 and 360
    Eigen::Matrix3f m;
     m = T.topLeftCorner(3,3);
     AngleAxisf aa(m);

     cout << " The aa axis : " << aa.axis() << endl;
     cout << " The aa angle in degrees : " <<aa.angle()*180/3.14157 << endl;

     theta=aa.angle();
     cout << " --- END Get Rotations --- " << endl;
*/
    Point pointXY;
    pointXY.x=standardcloud.points[0].x;
    pointXY.y=standardcloud.points[0].y;

    returnInfo.first= pointXY;

    returnInfo.second = theta;
    return returnInfo;

}

double Track::getScale(int idx)
{
    // TODO: remove or handle transformation with a scale DOF
    int theint=idx;
    theint++;
    return 1;
}

double Track::getRotationAngle(int idx)
{

    //cout << " --- Get Rotations --- " << endl;

    if (idx < 0) idx = frameIndex;
    Eigen::Matrix4f T = absoluteTransforms[idx-birthFrameIndex-1];
    float theta;

    //Manually create a cloud and shift
    pcl::PointCloud<pcl::PointXYZ>  standardcloud;




    // make the angle match convention of pointing down X axis = 0 degrees, poiting down positive y axis = 90 degrees

    pcl::PointXYZ p1,p2;
    p1.x=0; p1.y=0;
    p2.x=100; p2.y=0;

    standardcloud.push_back(p1);
    standardcloud.push_back(p2);
    // transformCloud(standardcloud, T);
    pcl::transformPointCloud(standardcloud,standardcloud, T);

    theta = atan2((float)(standardcloud.points[1].y-standardcloud.points[0].y), (float)(standardcloud.points[1].x-standardcloud.points[0].x)); // changed this function to make consistent
    //  cout << " The manually translated angle in degrees : " <<theta*180/3.14157 << endl;

    /* I don't know why this doesn't work for angles between 240 and 360
    Eigen::Matrix3f m;
     m = T.topLeftCorner(3,3);
     AngleAxisf aa(m);

     cout << " The aa axis : " << aa.axis() << endl;
     cout << " The aa angle in degrees : " <<aa.angle()*180/3.14157 << endl;

     theta=aa.angle();
     cout << " --- END Get Rotations --- " << endl;
*/
    return theta;


}

/**
 * Given a vector of Points, transform them and return them as an out param.
 */
void Track::getTemplatePoints(pcl::PointCloud<pcl::PointXYZRGB> &modelPts, int idx)
{
    if (idx < 0) idx = frameIndex;
    Eigen::Matrix4f T = absoluteTransforms[idx-birthFrameIndex-1];
    // transformCloud(modelPts,T);
    pcl::transformPointCloud(modelPts,modelPts, T);

    return;
}

/**
 * Given a cloud of datapts (an in/out variable), a vector of dataPts, an index to a specific
 * point in datapts and a distanceThreshold, add the list of indices into dataPts which are
 * within the distanceThreshold  to dirtyPts.
 *
 * @param dataPTSreduced_cloud a reference to a vector of Points
  * @param distanceThreshold an integer Euclidean distance
 */
pcl::PointCloud<pcl::PointXYZRGB> Track::removeClosestDataCloudPoints(pcl::PointCloud<pcl::PointXYZRGB> point_cloud_for_reduction,pcl::PointCloud<pcl::PointXYZRGB> removal_Cloud, int distanceThreshold){

        //NOTE: you cannot feed a KNN searcher clouds with 1 or fewer datapoints!

        //KDTREE SEARCH

        pcl::KdTreeFLANN<pcl::PointXYZRGB> kdtree;
        // Neighbors within radius search

        std::vector<int> pointIdxRadiusSearch;
        std::vector<float> pointRadiusSquaredDistance;

        float point_radius = distanceThreshold;

        PointCloud<pcl::PointXYZRGB> point_cloud_for_return;
        point_cloud_for_return.reserve(point_cloud_for_reduction.size());

        // K nearest neighbor search with Radius

        if(point_cloud_for_reduction.size()>1){

            bool *marked= new bool[point_cloud_for_reduction.size()];
            memset(marked,false,sizeof(bool)*point_cloud_for_reduction.size());
    //        for(uint q=0; q< point_cloud_for_reduction.size(); q++){
    //            marked[q]=false;

    //        }

            pcl:: PointCloud<PointXYZRGB>::Ptr point_cloud_for_reduction_ptr (new pcl::PointCloud<PointXYZRGB> (point_cloud_for_reduction));


            kdtree.setInputCloud (point_cloud_for_reduction_ptr); //Needs to have more than 1 data pt or segfault


            // iterate over points in model and remove those points within a certain distance
            for (unsigned int c=0; c < removal_Cloud.size(); c++)
            {

                if(point_cloud_for_reduction.size()<2){
                    break;
                }


                pcl::PointXYZRGB searchPoint;

                searchPoint.x = removal_Cloud.points[c].x;
                searchPoint.y = removal_Cloud.points[c].y;
                searchPoint.z = removal_Cloud.points[c].z;

                // qDebug() <<"Datapts before incremental remove"<< point_cloud_for_reduction.size();
                if ( kdtree.radiusSearch (searchPoint, point_radius, pointIdxRadiusSearch, pointRadiusSquaredDistance) > 0 )
                {
                    for (size_t i = 0; i < pointIdxRadiusSearch.size (); ++i){
                        if(point_cloud_for_reduction.size()>0) ///NOTE CHANGED FROM > 1

                        marked[pointIdxRadiusSearch[i]]=true;
    //                        point_cloud_for_reduction.erase(point_cloud_for_reduction.begin()+pointIdxRadiusSearch[i]);// point_cloud_for_reduction.points[ pointIdxRadiusSearch[i] ]
                    }
                }


            }



            for(uint q=0; q< point_cloud_for_reduction.size(); q++){
                if(!marked[q]){
                    point_cloud_for_return.push_back(point_cloud_for_reduction.at(q));
    //                point_cloud_for_return.at(q) = point_cloud_for_reduction.at(q);

                }

            }



            delete[] marked;
        }

        //point_cloud_for_reduction.resize();
        return point_cloud_for_return;

    }





//Functions for Correspondences API

////////////////////////////////////////////////////////////////////////////////
void Track::
estimateKeypoints (const PointCloud<PointXYZ>::Ptr &src,
                   const PointCloud<PointXYZ>::Ptr &tgt,
                   PointCloud<PointXYZ> &keypoints_src,
                   PointCloud<PointXYZ> &keypoints_tgt)
{
  PointCloud<int> keypoints_src_idx, keypoints_tgt_idx;
  // Get an uniform grid of keypoints
  UniformSampling<PointXYZ> uniform;
  uniform.setRadiusSearch (matchDistanceThreshold);  // 1m

  uniform.setInputCloud (src);
  uniform.compute (keypoints_src_idx);
  copyPointCloud<PointXYZ, PointXYZ> (*src, keypoints_src_idx.points, keypoints_src);

  uniform.setInputCloud (tgt);
  uniform.compute (keypoints_tgt_idx);
  copyPointCloud<PointXYZ, PointXYZ> (*tgt, keypoints_tgt_idx.points, keypoints_tgt);

  // For debugging purposes only: uncomment the lines below and use pcd_viewer to view the results, i.e.:
  // pcd_viewer source_pcd keypoints_src.pcd -ps 1 -ps 10
  //savePCDFileBinary ("keypoints_src.pcd", keypoints_src);
 // savePCDFileBinary ("keypoints_tgt.pcd", keypoints_tgt);
}

////////////////////////////////////////////////////////////////////////////////
void Track::
estimateNormals (const PointCloud<PointXYZ>::Ptr &src,
                 const PointCloud<PointXYZ>::Ptr &tgt,
                 PointCloud<Normal> &normals_src,
                 PointCloud<Normal> &normals_tgt)
{
  NormalEstimation<PointXYZ, Normal> normal_est;
  normal_est.setInputCloud (src);
  normal_est.setRadiusSearch (matchDistanceThreshold);  // 50cm
  normal_est.compute (normals_src);

  normal_est.setInputCloud (tgt);
  normal_est.compute (normals_tgt);

 /* // For debugging purposes only: uncomment the lines below and use pcd_viewer to view the results, i.e.:
  // pcd_viewer normals_src.pcd
  PointCloud<PointNormal> s, t;
  copyPointCloud<PointXYZ, PointNormal> (*src, s);
  copyPointCloud<Normal, PointNormal> (normals_src, s);
  copyPointCloud<PointXYZ, PointNormal> (*tgt, t);
  copyPointCloud<Normal, PointNormal> (normals_tgt, t);
  //savePCDFileBinary ("normals_src.pcd", s);
  //savePCDFileBinary ("normals_tgt.pcd", t);*/
}

////////////////////////////////////////////////////////////////////////////////
void Track::
estimateFPFH (const PointCloud<PointXYZ>::Ptr &src,
              const PointCloud<PointXYZ>::Ptr &tgt,
              const PointCloud<Normal>::Ptr &normals_src,
              const PointCloud<Normal>::Ptr &normals_tgt,
              const PointCloud<PointXYZ>::Ptr &keypoints_src,
              const PointCloud<PointXYZ>::Ptr &keypoints_tgt,
              PointCloud<FPFHSignature33> &fpfhs_src,
              PointCloud<FPFHSignature33> &fpfhs_tgt)
{
  FPFHEstimation<PointXYZ, Normal, FPFHSignature33> fpfh_est;
  fpfh_est.setInputCloud (keypoints_src);
  fpfh_est.setInputNormals (normals_src);
  fpfh_est.setRadiusSearch (matchDistanceThreshold); // 1m
  fpfh_est.setSearchSurface (src);
  fpfh_est.compute (fpfhs_src);

  fpfh_est.setInputCloud (keypoints_tgt);
  fpfh_est.setInputNormals (normals_tgt);
  fpfh_est.setSearchSurface (tgt);
  fpfh_est.compute (fpfhs_tgt);

  // For debugging purposes only: uncomment the lines below and use pcd_viewer to view the results, i.e.:
  /*// pcd_viewer fpfhs_src.pcd
  PointCloud2 s, t, out;
  toROSMsg (*keypoints_src, s); toROSMsg (fpfhs_src, t); concatenateFields (s, t, out);
  savePCDFile ("fpfhs_src.pcd", out);
  toROSMsg (*keypoints_tgt, s); toROSMsg (fpfhs_tgt, t); concatenateFields (s, t, out);
  savePCDFile ("fpfhs_tgt.pcd", out);*/
}

////////////////////////////////////////////////////////////////////////////////
void Track::
findCorrespondences (const PointCloud<FPFHSignature33>::Ptr &fpfhs_src,
                     const PointCloud<FPFHSignature33>::Ptr &fpfhs_tgt,
                     Correspondences &all_correspondences)
{
  pcl::registration::CorrespondenceEstimation <FPFHSignature33, FPFHSignature33> est;
  est.setInputCloud (fpfhs_src);
  est.setInputTarget (fpfhs_tgt);
  est.determineReciprocalCorrespondences (all_correspondences);
}

////////////////////////////////////////////////////////////////////////////////
void Track::
rejectBadCorrespondences (const CorrespondencesPtr &all_correspondences,
                          const PointCloud<PointXYZ>::Ptr &keypoints_src,
                          const PointCloud<PointXYZ>::Ptr &keypoints_tgt,
                          Correspondences &remaining_correspondences)
{
//  pcl::registration::CorrespondenceRejectorMedianDistance rej; //they didn't just have rejector distance
  pcl::registration::CorrespondenceRejectorDistance rej;
  rej.setInputCloud<PointXYZ> (keypoints_src);
  rej.setInputTarget<PointXYZ> (keypoints_tgt);
  //rej.setMedianFactor(matchDistanceThreshold);
  rej.setMaximumDistance (matchDistanceThreshold);    // 1m
  rej.setInputCorrespondences (all_correspondences);
  rej.getCorrespondences (remaining_correspondences);
}


////////////////////////////////////////////////////////////////////////////////
void Track::
computeTransformation (const PointCloud<PointXYZ>::Ptr &src,
                       const PointCloud<PointXYZ>::Ptr &tgt,
                       Eigen::Matrix4f &transform)
{
  // Get an uniform grid of keypoints
  PointCloud<PointXYZ>::Ptr keypoints_src (new PointCloud<PointXYZ>),
                            keypoints_tgt (new PointCloud<PointXYZ>);

  estimateKeypoints (src, tgt, *keypoints_src, *keypoints_tgt);
  //print_info ("Found %zu and %zu keypoints for the source and target datasets.\n", keypoints_src->points.size (), keypoints_tgt->points.size ());

  // Compute normals for all points keypoint
  PointCloud<Normal>::Ptr normals_src (new PointCloud<Normal>),
                          normals_tgt (new PointCloud<Normal>);
  estimateNormals (src, tgt, *normals_src, *normals_tgt);
  //print_info ("Estimated %zu and %zu normals for the source and target datasets.\n", normals_src->points.size (), normals_tgt->points.size ());

  // Compute FPFH features at each keypoint
  PointCloud<FPFHSignature33>::Ptr fpfhs_src (new PointCloud<FPFHSignature33>),
                                   fpfhs_tgt (new PointCloud<FPFHSignature33>);
  estimateFPFH (src, tgt, normals_src, normals_tgt, keypoints_src, keypoints_tgt, *fpfhs_src, *fpfhs_tgt);

  // Copy the data and save it to disk
/*  PointCloud<PointNormal> s, t;
  copyPointCloud<PointXYZ, PointNormal> (*keypoints_src, s);
  copyPointCloud<Normal, PointNormal> (normals_src, s);
  copyPointCloud<PointXYZ, PointNormal> (*keypoints_tgt, t);
  copyPointCloud<Normal, PointNormal> (normals_tgt, t);
*/



  // Find correspondences between keypoints in FPFH space
  CorrespondencesPtr all_correspondences (new Correspondences),
                     good_correspondences (new Correspondences);
  findCorrespondences (fpfhs_src, fpfhs_tgt, *all_correspondences);

  // Reject correspondences based on their XYZ distance
  rejectBadCorrespondences (all_correspondences, keypoints_src, keypoints_tgt, *good_correspondences);

  for (int i = 0; i < good_correspondences->size (); ++i)
    std::cerr << good_correspondences->at (i) << std::endl;
  // Obtain the best transformation between the two sets of keypoints given the remaining correspondences
  pcl::registration::TransformationEstimationSVD<PointXYZ, PointXYZ> trans_est;
  trans_est.estimateRigidTransformation (*keypoints_src, *keypoints_tgt, *good_correspondences, transform);
}


