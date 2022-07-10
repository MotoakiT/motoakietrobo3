#include "driving.h"


WheelsControl::WheelsControl(MotorIo* motor_io) : motor_io_(motor_io) {
}

void WheelsControl::Exec(int8_t target_power_l, int8_t target_power_r) {
  ///高橋
  counts_r_ = motor_io_->counts_r_;
  counts_l_ = motor_io_->counts_l_;
  ///
  int8_t curr_power_l = motor_io_->power_l_;
  if (target_power_l > curr_power_l) {
    curr_power_l += 1;
  } else if (target_power_l < curr_power_l) {
    curr_power_l -= 1;
  }

  int8_t curr_power_r = motor_io_->power_r_;
  if (target_power_r > curr_power_r) {
    curr_power_r += 1;
  } else if (target_power_r < curr_power_r) {
    curr_power_r -= 1;
  }

  if (target_power_l == 0 && target_power_r == 0) {
    motor_io_->StopWheels(true);
  } else {
    motor_io_->SetWheelsPower(curr_power_l, curr_power_r);
  }
}

BasicDriver::BasicDriver(WheelsControl* wheels_control)
    : wheels_control_(wheels_control),
      move_type_(kInvalidMove), base_power_(0) {
}

BasicDriver::~BasicDriver() {
}

void BasicDriver::SetParam(Move move_type, int8_t base_power) {
  move_type_ = move_type;
  base_power_ = base_power;
}

void BasicDriver::Run() {
  int8_t power_l;
  int8_t power_r;
  int32_t counts_r_ = wheels_control_ -> counts_r_;
  int32_t counts_l_ = wheels_control_ -> counts_l_;
  now_angle_r_[basepower_index] = counts_r_;
  now_angle_l_[basepower_index] = counts_l_;

  if (move_type_ == kGoForward) {
    target_value_speed = 1000;
    if(basepower_index < 5){
      for(int i = 0;i < base_power_index + 1;i ++){
        angle_least_squares[0][i] = now_angle_r_[i];
        angle_least_squares[1][i] = now_angle_l_[i];
      }else{
        for(int i = 0;i < 5;i ++){
        angle_least_squares[0][i] = now_angle_r_[basepower_index - i];
        angle_least_squares[1][i] = now_angle_l_[basepower_index - i];
        }
      }
    }

    for(i=0; i<5; i++){
      Sx[0] += angle_least_squares[0][i];
      Sx[1] += angle_least_squares[1][i];
      Sy[0] += time_index[i];
      Sy[1] += time_index[i];
      Sxy[0] += (angle_least_squares[0][i]*time_index[i]);
      Sxy[1] += (angle_least_squares[1][i]*time_index[i]);
      Sxx[0] += (angle_least_squares[0][i]*angle_least_squares[0][i]);
      Sxx[1] += (angle_least_squares[0][i]*angle_least_squares[1][i]);
      }

    D[0] = 5*Sxx[0] - Sx[0]*Sx[0];
    D[1] = 5*Sxx[1] - Sx[1]*Sx[1];
    now_speed_l = (Sxx[0]*Sy[0]-Sxy[0]*Sx[0])/D[0];
    now_speed_r = (Sxx[1]*Sy[1]-Sxy[1]*Sx[1])/D[1];

    error_now[0][basepower_index] = target_value_speed - now_speed_l;
    error_now[1][basepower_index] = target_value_speed - now_speed_r;
    error_interal[0] += (error_now[0][basepower_index] + error_now[0][basepower_index])/(2*delta_t_pid);
    error_interal[1] += (error_now[1][basepower_index] + error_now[1][basepower_index])/(2*delta_t_pid);
    error_differential[0] = (error_now[0][basepower_index] - error_now[0][basepower_index - 1])/delta_t_pid;
    error_differential[1] = (error_now[1][basepower_index] - error_now[1][basepower_index - 1])/delta_t_pid;

    Kp = {2.0,2.0};
    Ki = {0.0,0.0};
    Kd = {0.0,0.0};

    motor_power_pid[0] = Kp*error_now[0][basepower_index] + error_interal[0]*Ki + error_differential[0]Kd;
    motor_power_pid[1] = Kp*error_now[1][basepower_index] + error_interal[1]*Ki + error_differential[1]Kd;

    power_l = motor_power_pid[0];
    power_r = motor_power_pid[1];
  } else if (move_type_ == kGoBackward) {
    power_l = power_r = -base_power_;
  } else if (move_type_ == kRotateLeft) {
    power_l = -base_power_;
    power_r = base_power_;
  } else if (move_type_ == kRotateRight) {
    power_l = base_power_;
    power_r = -base_power_;
  } else {
    power_l = power_r = 0;
  }

  wheels_control_->Exec(power_l, power_r);
  basepower_index += 1;
}

void BasicDriver::Stop() {
  wheels_control_->Exec(0, 0);
}

///高橋
void BasicDriver::SaveBasePower(){
  char str [256];
  FILE* fp = fopen("error_motoaki.csv", "w");

  for (int i=0; i < basepower_index;  i++) {
    //sprintf(str, "%d,%f,%f,%d\n", power_r_[i],now_apt_r_[i],error_now_r_[i],now_angle_r_[i]);
    sprintf(str, "%d\n", now_angle_r_[i]);
    fprintf(fp, str);
  }

  fclose(fp);
}
///

LineTracer::LineTracer(WheelsControl* wheels_control, Luminous* luminous)
    : wheels_control_(wheels_control), luminous_(luminous),
      move_type_(kInvalidMove), base_power_(0) {
  pid_control_ = new PidControl();
}

LineTracer::~LineTracer() {
  delete pid_control_;
}

void LineTracer::SetParam(Move move_type, int8_t base_power, Gain gain) {
  move_type_ = move_type;
  base_power_ = base_power;
  pid_control_->SetGain(gain.kp, gain.ki, gain.kd);
}

void LineTracer::Run() {
  float curr_hsv = luminous_->hsv_.v;
  float mv = pid_control_->CalcMv(line_trace_threshold, curr_hsv);

  if (move_type_ == kTraceLeftEdge) {
    mv *= -1;
  }

  int8_t power_l = static_cast<int8_t>(base_power_ + mv);
  int8_t power_r = static_cast<int8_t>(base_power_ - mv);
  wheels_control_->Exec(power_l, power_r);
}

void LineTracer::Stop() {
  wheels_control_->Exec(0, 0);
}

EndCondition::EndCondition(Luminous* luminous, Localize* localize)
    : luminous_(luminous), localize_(localize),
      end_type_(kInvalidEnd), end_color_(kInvalidColor), end_threshold_(0),
      end_state_(false), ref_distance_(0), ref_theta_(0) {
}

void EndCondition::SetParam(End end_type, Color end_color, float end_threshold) {
  end_type_ = end_type;
  end_color_ = end_color;
  end_threshold_ = end_threshold;
  end_state_ = false;

  // if (end_type_ == kDistanceEnd) {
  //   ref_distance_ = localize_->distance_;
  // } else if (end_type_ == kThetaEnd) {
  //   ref_theta_ = localize_->pose_.theta;
  // }
}

bool EndCondition::IsSatisfied() {
  switch (end_type_) {
    case kColorEnd:
      if (end_color_ == luminous_->color_)
        end_state_ = true;
      break;

    // case kDistanceEnd:
    //   if (end_threshold_ > 0 && localize_->distance_ - ref_distance_ > end_threshold_)
    //     end_state_ = true;
    //   else if (end_threshold_ < 0 && localize_->distance_ - ref_distance_ < end_threshold_)
    //     end_state_ = true;
    //   break;

    // case kThetaEnd:
    //   if (end_threshold_ > 0 && localize_->pose_.theta - ref_theta_ > end_threshold_)
    //     end_state_ = true;
    //   else if (end_threshold_ < 0 && localize_->pose_.theta - ref_theta_ < end_threshold_)
    //     end_state_ = true;
    //   break;

    default:
      break;
  }

  return end_state_;
}

DrivingManager::DrivingManager(BasicDriver* basic_driver, LineTracer* line_tracer, EndCondition* end_condition)
    : basic_driver_(basic_driver), line_tracer_(line_tracer), end_condition_(end_condition) {
}

void DrivingManager::Update() {
  if (is_satisfied) {
    return;
  }

  Drive(curr_param);

  if (end_condition_->IsSatisfied()) {
    is_satisfied = true;
  }
}

void DrivingManager::SetDriveParam(DrivingParam param) {
  is_satisfied = false;
  curr_param = param;
  SetMoveParam(curr_param);
  SetEndParam(curr_param);
}

void DrivingManager::SetMoveParam(DrivingParam& param) {
  Move move_type = param.move_type;
  int8_t base_power = param.base_power;
  Gain gain = param.gain;

  switch (move_type) {
    case kTraceLeftEdge:
    case kTraceRightEdge:
      line_tracer_->SetParam(move_type, base_power, gain);
      break;

    case kGoForward:
    case kGoBackward:
    case kRotateLeft:
    case kRotateRight:
      basic_driver_->SetParam(move_type, base_power);
      break;

    default:
      break;
  }
}

void DrivingManager::SetEndParam(DrivingParam& param) {
  End end_type = param.end_type;
  Color end_color = param.end_color;
  float end_threshold = param.end_threshold;

  end_condition_->SetParam(end_type, end_color, end_threshold);
}

void DrivingManager::Drive(DrivingParam& param) {
  Move move_type = param.move_type;

  switch (move_type) {
    case kTraceLeftEdge:
    case kTraceRightEdge:
      line_tracer_->Run();
      break;

    case kGoForward:
    case kGoBackward:
    case kRotateLeft:
    case kRotateRight:
      basic_driver_->Run();
      break;

    case kStopWheels:
      basic_driver_->Stop();
      break;

    default:
      break;
  }
}
