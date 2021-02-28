#ifndef GAIT_CTRLLER_H
#define GAIT_CTRLLER_H

#include <math.h>
#include <time.h>

#include <iostream>
#include <string>

#include "Controllers/ContactEstimator.h"
#include "Controllers/ControlFSMData.h"
#include "Controllers/DesiredStateCommand.h"
#include "Controllers/OrientationEstimator.h"
#include "Controllers/PositionVelocityEstimator.h"
#include "Controllers/RobotLegState.h"
#include "Controllers/StateEstimatorContainer.h"
#include "Controllers/SafetyChecker.h"
#include "Dynamics/MiniCheetah.h"
#include "MPC_Ctrl/ConvexMPCLocomotion.h"
#include "Utilities/IMUTypes.h"
#include "calculateTool.h"

////add by shimizu
// #include <zebra_msgs/ZebraJointControl.h>

struct JointEff
{
  double eff[12];
};

//// add by shimizu
struct Zebra
{
  double position[12];
  double velocity[12];
  double kp[12];
  double kd[12];
  double effort[12];
};

class GaitCtrller
{
public:
  GaitCtrller(double freq, double *PIDParam);
  ~GaitCtrller();
  void SetIMUData(double *imuData);
  void SetLegData(double *motorData);
  void PreWork(double *imuData, double *motorData);
  void SetGaitType(int gaitType);
  void SetRobotMode(int mode);
  void SetRobotVel(double *vel);
  void ToqueCalculator(double *imuData, double *motorData, double *effort);
  LegControllerCommand<float> GetLegControllerCommand(int leg)
  {
    return _legController->commands[leg];
  }
  LegControllerData<float> GetLegControllerData(int leg)
  {
    return _legController->datas[leg];
  }
  bool GetSafetyCheck()
  {
    return _safetyCheck;
  }

private:
  int _gaitType = 0;
  int _robotMode = 0;
  bool _safetyCheck = true;
  std::vector<double> _gamepadCommand;
  Vec4<float> ctrlParam;

  Quadruped<float> _quadruped;
  ConvexMPCLocomotion *convexMPC;
  LegController<float> *_legController;
  StateEstimatorContainer<float> *_stateEstimator;
  LegData _legdata;
  LegCommand legcommand;
  ControlFSMData<float> control_data;
  VectorNavData _vectorNavData;
  CheaterState<double> *cheaterState;
  StateEstimate<float> _stateEstimate;
  RobotControlParameters *controlParameters;
  DesiredStateCommand<float> *_desiredStateCommand;
  SafetyChecker<float> *safetyChecker;
};

extern "C"
{

  GaitCtrller *gCtrller = NULL;
  JointEff jointEff;
  ////add by shimizu
  Zebra joint_control;

  // first step, init the controller
  void init_controller(double freq, double PIDParam[])
  {
    if (NULL != gCtrller)
    {
      delete gCtrller;
    }
    gCtrller = new GaitCtrller(freq, PIDParam);
  }

  // the kalman filter need to work second
  void pre_work(double imuData[], double legData[])
  {
    gCtrller->PreWork(imuData, legData);
  }

  // gait type can be set in any time
  void set_gait_type(int gaitType) { gCtrller->SetGaitType(gaitType); }

  // set robot mode, 0: High performance model, 1: Low power mode
  void set_robot_mode(int mode) { gCtrller->SetRobotMode(mode); }

  // robot vel can be set in any time
  void set_robot_vel(double vel[]) { gCtrller->SetRobotVel(vel); }

  // after init controller and pre work, the mpc calculator can work
  JointEff *toque_calculator(double imuData[], double motorData[])
  {
    double eff[12] = {0.0};
    gCtrller->ToqueCalculator(imuData, motorData, eff);
    for (int i = 0; i < 12; i++)
    {
      jointEff.eff[i] = eff[i];
    }
    // std::cout << "qDes"<<gCtrller->GetLegControllerCommand(0).kpCartesian<< std::endl;
    return &jointEff;
  }

  ////add by shimizu
  Zebra *get_zebra_joint_control()
  {
    // double eff[12] = {0.0};
    // gCtrller->ToqueCalculator(imuData, motorData, eff);
    bool safety_check = gCtrller->GetSafetyCheck();
    if (safety_check)
    {
      for (int leg = 0; leg < 4; leg++)
      {
        LegControllerCommand<float> command = gCtrller->GetLegControllerCommand(leg);
        LegControllerData<float> now_data = gCtrller->GetLegControllerData(leg);
        Vec3<float> legTorque = now_data.J.transpose() * command.forceFeedForward;

        for (int i = 0; i < 3; i++)
        {
          int n = 3 * leg + i;
          joint_control.position[n] = command.qDes[i];
          joint_control.velocity[n] = command.qdDes[i];
          joint_control.kp[n] = command.kpCartesian(0, 0);
          joint_control.kd[n] = command.kdCartesian(0, 0);
          joint_control.effort[n] = command.tauFeedForward[i] + legTorque[i];
        }
      }
    }
    else
    {
      for (int n = 0; n < 4 * 3; n++)
      {
        joint_control.kp[n] = 0;
        joint_control.kd[n] = 0;
        joint_control.effort[n] = 0;
      }
    }
    return &joint_control;
  }
}

#endif