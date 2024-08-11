#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <iostream>
#include <cstring>

using namespace std;

struct ForceFeedback {
    double torque;
    double position;
};



class G29ForceFeedback {

private:
    // ros::Subscriber sub_target;
    // ros::Timer timer;

    // device info
    int m_device_handle;
    int m_axis_code = ABS_X;
    int m_axis_min;
    int m_axis_max;

    // rosparam
    std::string m_device_name;
    double m_loop_rate;
    double m_max_torque;
    double m_min_torque;
    double m_brake_position;
    double m_brake_torque_rate;
    double m_auto_centering_max_torque;
    double m_auto_centering_max_position;
    double m_eps;
    bool m_auto_centering;

    // variables
    ForceFeedback m_target;
    bool m_is_target_updated = false;
    bool m_is_brake_range = false;
    struct ff_effect m_effect;
    double m_position;
    double m_torque;
    double m_attack_length;

public:
    G29ForceFeedback();
    ~G29ForceFeedback();
    void targetCallback(double c_position, double c_torque);
    void loop();

private:
    
    
    int testBit(int bit, unsigned char *array);
    void initDevice();
    void calcRotateForce(double &torque, double &attack_length, ForceFeedback &target, const double &current_position);
    void calcCenteringForce(double &torque, ForceFeedback &target, const double &current_position);
    void uploadForce(const double &position, const double &force, const double &attack_length);
};


G29ForceFeedback::G29ForceFeedback() {

    // ros::NodeHandle n;
    // sub_target = n.subscribe("/ff_target", 1, &G29ForceFeedback::targetCallback, this);

    // n.getParam("device_name", m_device_name);
    // n.getParam("loop_rate", m_loop_rate);
    // n.getParam("max_torque", m_max_torque);
    // n.getParam("min_torque", m_min_torque);
    // n.getParam("brake_position", m_brake_position);
    // n.getParam("brake_torque_rate", m_brake_torque_rate);
    // n.getParam("auto_centering_max_torque", m_auto_centering_max_torque);
    // n.getParam("auto_centering_max_position", m_auto_centering_max_position);
    // n.getParam("eps", m_eps);
    // n.getParam("auto_centering", m_auto_centering);

    m_device_name = "/dev/input/by-id/usb-Logitech_G29_Driving_Force_Racing_Wheel-event-joystick";
    m_loop_rate = 1;
    m_max_torque = 1.0;
    m_min_torque = 0.2;
    m_brake_torque_rate = 0.2;
    m_brake_position = 0.1;
    m_auto_centering_max_torque = 0.3;
    m_auto_centering_max_position = 0.2;
    m_eps = 0.01;
    m_auto_centering = true;


    initDevice();

    // ros::Duration(1).sleep();
    // timer = n.createTimer(ros::Duration(m_loop_rate), &G29ForceFeedback::loop, this);
}

G29ForceFeedback::~G29ForceFeedback() {

    m_effect.type = FF_CONSTANT;
    m_effect.id = -1;
    m_effect.u.constant.level = 0;
    m_effect.direction = 0;
    // upload m_effect
    if (ioctl(m_device_handle, EVIOCSFF, &m_effect) < 0) {
        std::cout << "failed to upload m_effect" << std::endl;
    }
}


// update input event with timer callback
void G29ForceFeedback::loop() {

    struct input_event event;
    double last_position = m_position;
    // get current state
    while (read(m_device_handle, &event, sizeof(event)) == sizeof(event)) {
        if (event.type == EV_ABS && event.code == m_axis_code) {
            m_position = (event.value - (m_axis_max + m_axis_min) * 0.5) * 2 / (m_axis_max - m_axis_min);
        }
    }

    if (m_is_brake_range || m_auto_centering) {
        calcCenteringForce(m_torque, m_target, m_position);
        m_attack_length = 0.0;

    } else {
        calcRotateForce(m_torque, m_attack_length, m_target, m_position);
        m_is_target_updated = false;
    }

    uploadForce(m_target.position, m_torque, m_attack_length);
}


void G29ForceFeedback::calcRotateForce(double &torque,
                                       double &attack_length,
                                       ForceFeedback &target,
                                       const double &current_position) {

    double diff = target.position - current_position;
    double direction = (diff > 0.0) ? 1.0 : -1.0;

    if (fabs(diff) < m_eps) {
        torque = 0.0;
        attack_length = 0.0;

    } else if (fabs(diff) < m_brake_position) {
        m_is_brake_range = true;
        torque = target.torque * m_brake_torque_rate * -direction;
        attack_length = m_loop_rate;

    } else {
        torque = target.torque * direction;
        attack_length = m_loop_rate;
    }

    // std::cout << diff << direction << torque << attack_length << std::endl;
}


void G29ForceFeedback::calcCenteringForce(double &torque,
                                          ForceFeedback &target,
                                          const double &current_position) {

    double diff = target.position - current_position;
    double direction = (diff > 0.0) ? 1.0 : -1.0;

    if (fabs(diff) < m_eps)
        torque = 0.0;

    else {
        double torque_range = m_auto_centering_max_torque - m_min_torque;
        double power = (fabs(diff) - m_eps) / (m_auto_centering_max_position - m_eps);
        double buf_torque = power * torque_range + m_min_torque;
        torque = std::min(buf_torque, m_auto_centering_max_torque) * direction;
    }
}


// update input event with writing information to the event file
void G29ForceFeedback::uploadForce(const double &position,
                                   const double &torque,
                                   const double &attack_length) {

    //std::cout << torque << std::endl;
    // set effect
    m_effect.u.constant.level = 0x7fff * std::min(torque, m_max_torque);
    m_effect.direction = 0xC000;
    m_effect.u.constant.envelope.attack_level = 0; /* 0x7fff * force / 2 */
    m_effect.u.constant.envelope.attack_length = attack_length;
    m_effect.u.constant.envelope.fade_level = 0;
    m_effect.u.constant.envelope.fade_length = attack_length;

    // upload effect
    if (ioctl(m_device_handle, EVIOCSFF, &m_effect) < 0) {
        std::cout << "failed to upload effect" << std::endl;
    }
}


// get target information of wheel control from ros message
void G29ForceFeedback::targetCallback(double c_position, double c_torque) {

    if (m_target.position == c_position && m_target.torque == fabs(c_torque)) {
        m_is_target_updated = false;

    } else {
        m_target.torque = c_torque;
        m_target.position = c_position;
        m_target.torque = fabs(m_target.torque);
        m_is_target_updated = true;
        m_is_brake_range = false;
    }
}


// initialize force feedback device
void G29ForceFeedback::initDevice() {
    // setup device
    unsigned char key_bits[1+KEY_MAX/8/sizeof(unsigned char)];
    unsigned char abs_bits[1+ABS_MAX/8/sizeof(unsigned char)];
    unsigned char ff_bits[1+FF_MAX/8/sizeof(unsigned char)];
    struct input_event event;
    struct input_absinfo abs_info;

    m_device_handle = open(m_device_name.c_str(), O_RDWR|O_NONBLOCK);
    if (m_device_handle < 0) {
        std::cout << "ERROR: cannot open device : "<< m_device_name << std::endl;
        std::cout << "Check G29 is connected and permission is set." << std::endl;
        
        // signal to py clib raise error
        return;

    } else {std::cout << "device opened" << std::endl;}

    // which axes has the device?
    memset(abs_bits, 0, sizeof(abs_bits));
    if (ioctl(m_device_handle, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) < 0) {
        std::cout << "ERROR: cannot get abs bits" << std::endl;
        exit(1);
    }

    // get some information about force feedback
    memset(ff_bits, 0, sizeof(ff_bits));
    if (ioctl(m_device_handle, EVIOCGBIT(EV_FF, sizeof(ff_bits)), ff_bits) < 0) {
        std::cout << "ERROR: cannot get ff bits" << std::endl;
        exit(1);
    }

    // get axis value range
    if (ioctl(m_device_handle, EVIOCGABS(m_axis_code), &abs_info) < 0) {
        std::cout << "ERROR: cannot get axis range" << std::endl;
        exit(1);
    }
    m_axis_max = abs_info.maximum;
    m_axis_min = abs_info.minimum;
    if (m_axis_min >= m_axis_max) {
        std::cout << "ERROR: axis range has bad value" << std::endl;
        exit(1);
    }

    // check force feedback is supported?
    if(!testBit(FF_CONSTANT, ff_bits)) {
        std::cout << "ERROR: force feedback is not supported" << std::endl;
        exit(1);

    } else { std::cout << "force feedback supported" << std::endl; }

    // auto centering off
    memset(&event, 0, sizeof(event));
    event.type = EV_FF;
    event.code = FF_AUTOCENTER;
    event.value = 0;
    if (write(m_device_handle, &event, sizeof(event)) != sizeof(event)) {
        std::cout << "failed to disable auto centering" << std::endl;
        exit(1);
    }

    std::cout << "Init Device Done." << std::endl;

    // init effect and get effect id
    memset(&m_effect, 0, sizeof(m_effect));
    m_effect.type = FF_CONSTANT;
    m_effect.id = -1; // initial value
    m_effect.trigger.button = 0;
    m_effect.trigger.interval = 0;
    m_effect.replay.length = 0xffff;  // longest value
    m_effect.replay.delay = 0; // delay from write(...)
    m_effect.u.constant.level = 0;
    m_effect.direction = 0xC000;
    m_effect.u.constant.envelope.attack_length = 0;
    m_effect.u.constant.envelope.attack_level = 0;
    m_effect.u.constant.envelope.fade_length = 0;
    m_effect.u.constant.envelope.fade_level = 0;

    if (ioctl(m_device_handle, EVIOCSFF, &m_effect) < 0) {
        std::cout << "failed to upload m_effect" << std::endl;
        exit(1);
    }

    // start m_effect
    memset(&event, 0, sizeof(event));
    event.type = EV_FF;
    event.code = m_effect.id;
    event.value = 1;
    if (write(m_device_handle, &event, sizeof(event)) != sizeof(event)) {
        std::cout << "failed to start event" << std::endl;
        exit(1);
    }
}


// util for initDevice()
int G29ForceFeedback::testBit(int bit, unsigned char *array) {

    return ((array[bit / (sizeof(unsigned char) * 8)] >> (bit % (sizeof(unsigned char) * 8))) & 1);
}


int main(int argc, char **argv ){
    int cnt = -430;
    G29ForceFeedback g29_ff;
    while (1) {
        g29_ff.loop();
        g29_ff.targetCallback(-50.0, 0.4);
        sleep(1);
    }
    
    
    //ros::spin();
    return(0);
}


extern "C"
{
    G29ForceFeedback* g29_ff() {return new G29ForceFeedback();}
    void g29_loop(G29ForceFeedback* g29_ff) {g29_ff->loop();}
    //int Foo_foobar(Foo* foo, int n) {return foo->foobar(n);}
}
