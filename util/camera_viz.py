import cv2
import numpy as np
import copy

SR =  28 # steering ratio

def overlay_transparent(background, overlay, x, y):
    background_width = background.shape[1]
    background_height = background.shape[0]

    if x >= background_width or y >= background_height:
        return background

    h, w = overlay.shape[0], overlay.shape[1]

    if x + w > background_width:
        w = background_width - x
        overlay = overlay[:, :w]

    if y + h > background_height:
        h = background_height - y
        overlay = overlay[:h]

    if overlay.shape[2] < 4:
        overlay = np.concatenate(
            [
                overlay,
                np.ones((overlay.shape[0], overlay.shape[1], 1), dtype=overlay.dtype) * 255
            ],
            axis=2,
        )

    overlay_image = overlay[..., :3]
    mask = overlay[..., 3:] / 255.0

    background[y:y+h, x:x+w] = (1.0 - mask) * background[y:y+h, x:x+w] + mask * overlay_image

    return background

def calc_dist(v,a,t):
    return v*t+a*(t**2)/2

def calc_camcoor_straight(width, speed, ax, t, cam_mat):
    cam_coor = np.array([[0,0,0]])
    for i in t:
        coor = np.array([[width, 1.2, calc_dist(speed,ax,i)]])
        cam_coor_temp = (cam_mat @ coor.T).T
        cam_coor_merge = np.r_[cam_coor,cam_coor_temp]
        cam_coor = cam_coor_merge

    return cam_coor

def calc_camcoor_turn(width, r, w, alpha, t, cam_mat, l = 1.5):
    cam_coor = np.array([[0,0,0]])

    for i in t:
        th = calc_dist(w,alpha,i)
        if th < np.pi/2 and th > np.pi/2*-1:
            coor = np.array([[ -r * (1 - np.cos(th)) - l * np.sin(th) + width, 1.2, r * np.sin(th) - l * (1-np.cos(th)) ]])
            cam_coor_temp = (cam_mat @ coor.T).T
            cam_coor_merge = np.r_[cam_coor,cam_coor_temp]
            cam_coor = cam_coor_merge

    return cam_coor

def plot_image(image2, cam_coor, t):
    
    for i in range(1,len(t)):
        image2 = cv2.line(image2,(np.int32(cam_coor[i,0]/cam_coor[i,2]),np.int32(cam_coor[i,1]/cam_coor[i,2])),
        (np.int32(cam_coor[i,0]/cam_coor[i,2]),np.int32(cam_coor[i,1]/cam_coor[i,2])),
        color=(0,255*(i-1)/(len(t)-2),255*(1-(i-1)/(len(t)-2))), thickness=10)

    return image2

def draw_car_guideline(image, car_width, offset, angle=-10, l = 1.5, speed=0, ax=0):
    cam_mat = np.array([[1047.2*0.6, 0, 1045.8*0.6],
                        [0, 1050.9*0.6, 649.4*0.6],
                        [0,0,1]] )
    speed = speed*1000/3600
    t = np.arange(0.1,2,0.05)

    delay = 0.3
    image2 = copy.deepcopy(image)
    angle_rad = np.deg2rad(angle)
    delay_index = int(delay*20)-2
    height, width, _ = image.shape

    if speed<3:
        speed =3
    
    if speed !=0:
        if angle == 0:
            cam_coor_l = calc_camcoor_straight(-car_width/2+offset, speed, ax, t, cam_mat)
            cam_coor_r = calc_camcoor_straight(car_width/2+offset, speed, ax, t, cam_mat)

        else:
            r = l / angle_rad
            w = speed / r
            alpha = ax/r
            cam_coor_l = calc_camcoor_turn(-car_width/2+offset,r,w,alpha,t,cam_mat, l)
            cam_coor_r = calc_camcoor_turn(car_width/2+offset,r,w,alpha,t,cam_mat, l)
	
        image2 = plot_image(image2, cam_coor_l,t)
        image2 = plot_image(image2, cam_coor_r,t)
        delay_point = np.array([[[np.int32(width/6)*1, height],
                             [np.int32(width/6)*5, height],
                             [np.int32(cam_coor_r[delay_index,0]/cam_coor_r[delay_index,2]), np.int32(cam_coor_r[delay_index,1]/cam_coor_r[delay_index,2])],
                             [np.int32(cam_coor_l[delay_index,0]/cam_coor_l[delay_index,2]), np.int32(cam_coor_l[delay_index,1]/cam_coor_l[delay_index,2])]
                             ]])
        image2 = cv2.fillPoly(image2, delay_point, (255, 255, 255))

    image3 = cv2.addWeighted(image2, 0.3, image, 0.7, 0)
    
    return image3

def draw_ax_steer_bar(image, ax, steer):
    max_steer = 450
    min_steer = -max_steer

    max_ax = 5 # m/s^2
    min_ax = -max_ax

    height, width, _ = image.shape
    image4 = copy.deepcopy(image)
    ax_bar_height = -(int((ax ) / (max_ax - min_ax) * height))
    ax_bar = np.array([[[width, height/2], [width - 5, height/2], [width - 5, height/2 + ax_bar_height/2], [width, height/2 + ax_bar_height/2]]], dtype=np.int32)
    image3 = cv2.fillPoly(image4, ax_bar, (0, 255, 0))


    steer_bar_width = -(int((steer) / (max_steer - min_steer) * width))
    steer_bar = np.array([[[width/2, height], [width/2 + steer_bar_width/2, height], [width/2 + steer_bar_width/2, height - 5], [width/2, height - 5]]], dtype=np.int32)
    image3 = cv2.fillPoly(image3, steer_bar, (0, 255, 255))


    return image3

def draw_rot_steer(image, steer):
    M = cv2.getRotationMatrix2D((cols/2, rows/2), angle, 1)

    steering_wheel = cv2.warpAffine(steering_wheel, M, (cols, rows))

    startY = int(0.8*image.shape[0])
    endY = image.shape[0]
    cv2.rectangle(overlay, (0, startY), (image.shape[1], endY), (0, 0, 0), -1)

    steering_wheel_resized = cv2.resize(steering_wheel, (300, 300))
    image = overlay_transparent(image, steering_wheel_resized, image.shape[1]-steering_wheel_resized.shape[1]-50, startY)
    

# image = cv2.imread('./sample.png')
# overlay = copy.deepcopy(image)

speed = '70 km/h'
gear = 'D3'
angle = -10 
# transparent image load

DEMOIMG = cv2.imread('./assets/steer.png', cv2.IMREAD_UNCHANGED)
 

rows, cols, _ = DEMOIMG.shape


def hud(angle, speed, gear, sri, bg, offset=0):
    steering_wheel = sri

    if sri is None:
        steering_wheel = DEMOIMG

    image = bg
    overlay = copy.deepcopy(image)
    M = cv2.getRotationMatrix2D((cols/2, rows/2), angle, 1)

    steering_wheel = cv2.warpAffine(steering_wheel, M, (cols, rows))

    startY = int(0.8*image.shape[0])
    endY = image.shape[0]
    cv2.rectangle(overlay, (0, startY), (image.shape[1], endY), (0, 0, 0), -1)

    alpha = 0.3
    image = cv2.addWeighted(overlay, alpha, image, 1 - alpha, 0)

    cv2.putText(image, 'Speed: ' + str(speed), (50, startY + 50), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2, cv2.LINE_AA)
    cv2.putText(image, 'Gear: ' + gear, (50, startY + 100), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2, cv2.LINE_AA)

    steering_wheel_resized = cv2.resize(steering_wheel, (300, 300))

    image = overlay_transparent(image, steering_wheel_resized, image.shape[1]-steering_wheel_resized.shape[1]-50, startY)

    image = draw_car_guideline(image, 1.4, offset, angle/SR, l = 1.5 , speed=speed, ax=0)

    image = draw_ax_steer_bar(image, speed , angle)

    return image

import time

if __name__ == "__main__":
    for i in range(-400, 400, 5):
        t1 =    time.time()
        cv2.imshow('image', hud(i, speed, gear, DEMOIMG, image))
        t2 =    time.time()
        print("time : ", t2 - t1)
        cv2.waitKey(0)