# Project: Water Meter Reading with Camera-based Index Recognition


## Table of Contents

1. [Description of the Project](#1-description-of-the-project)
   - [Equipment Used](#equipment-used)
   - [Software Used](#software-used)
2. [What is Done](#2-what-is-done)
   - [ESP32-CAM Code](#about-the-esp32-cam-code)
   - [Convolutional Neural Network](#about-the-cnn)
   - [Camera Support](#about-the-support-for-the-esp32-cam)
   - [Grafana](#about-the-grafana)
3. [How to Use/Launch the Project](#3-how-to-uselaunch-the-project)
   - [Arduino Code](#arduino-code)
   - [Convolutional Neural Network Code](#convolutional-neural-network-code)
   - [Docker Container](#docker-container)
4. [What is Missing (Bugs)](#4-what-is-missing-bugs)

##
# 1. Description of the project:

The aim of this project was to enable automatic reading of water meter readings, enabling monitoring and automatic triggering of alerts in the event of abnormal consumption. 

## Equipment used
For this project, we used an ESP32-CAM  
![alt text](https://asset.conrad.com/media10/isa/160267/c1/-/en/002332111PI00/image.jpg?x=400&y=400&format=jpg&ex=400&ey=400&align=center)

## Software used

In order to code on the ESP32-CAM we used the software called Arduino IDE
![alt text](https://ai.thestempedia.com/wp-content/uploads/2022/07/Arduino_Logo.svg_.jpg)
 
We also used Google collab in order to train a CNN model able to predict digits.
![alt text](https://colab.research.google.com/img/colab_favicon_256px.png)

# 2. What is done
## About the ESP32-CAM code

We developped a code able to take a picture with the flash LED of the ESP32-CAM, save it on the micro SD card, crop the original picture into 5 pictures (the 5 digits of the water meter) and save them on the micro SD. 

Then, we take these 5 pictures and apply the necessary preprocessing on them.

## About the CNN

We built our own dataset composed of digital digits but also half-half digits to solve the water meter display problem.
You can have access to the dataset in the ZIP file called "digits"

![alt text for screen readers](/picture_ReadMe/digit1.png "digit 1 from dataset")
![alt text for screen readers](/picture_ReadMe/digit1_merged.jpg "digit 1 merged from dataset")

In our notebook "CNN_MODEL" you will find the differents step to train the model with comments.
Here are the main steps:
- Import necessary libraries
- Load the dataset
- Apply a full preprocessing to each pictures of the dataset
- Build a model with complex layers
- Train this model with the datasets' pictures
- Converting a Keras Model to a C++ Representation

With our dataset, preprocessing and model we are able to reach a 99% of accuracy. 

## About the support for the ESP32-CAM

We created a sort of support in order to protect the camera and make it motionless.

![alt text for screen readers](/picture_ReadMe/support.jpg "support for the camera")

This support is made of 3 pieces in order to make it adjustable to have a better focus.


## About the Grafana

We developped a docker compose file composed of 3 services:
- Node-red, present for facilitate the connection between the LoRaWAN server and the database where values are stored 

![alt text for screen readers](/picture_ReadMe/node_red.png "node-red page")

- Postgres, which is the database used to store the values predicted and send by our ESP32-CAM
- Grafana, which is the service charged to get the values from the database and display them in order to keep track of consumption.


![alt text for screen readers](/picture_ReadMe/Grafana.png "Grafana page")

# 3. How to use/launch the project

## Arduino code

First, lets set up the arduino IDE if you never used a ESP32-CAM.

Follow this tutorial to set up the Arduino IDE.
https://randomnerdtutorials.com/installing-esp32-arduino-ide-2-0/

After that, you can follow this tutorial to test if everything is working.
https://randomnerdtutorials.com/esp32-cam-video-streaming-face-recognition-arduino-ide/

After doing that, you can download the .ino file from the github project. Upload the code on the ESP32-CAM and open the Serial Monitor.
Here you will get an IP adress 

![alt text for screen readers](/picture_ReadMe/ip_adress.png "ip adress")
Please visit this address in your browser and use the "Capture Image" button. This interface is provisional, designed to let you view the images captured by the ESP32-CAM without needing to remove the microSD card and connect it to your computer each time.
On this page, you can view the captured image along with the five preprocessed cropped images.

## Convolutional Neural Network code

If you want to take a look at the CNN code, you can download it from the github project. It is full of comments to help you understand how it is working. You also have to download the digits folder which is the dataset needed by the code to be able to train the model.
You can also use it to test the prediction on any pictures. (cell commented **"Manual Testing of the CNN Model"**)
The model has between 98 and 99% of accuracy and can easily predict digits even with average quality pictures.

## Docker container

In the github project, you can download the docker-compose file which is needed to launch the node-red, postgresSQL and grafana images.
Once downloaded, you have to type in a terminal the command: 
```
docker compose up -d --build
```

Once everything is launched, you can acces the node-red page at this adress: http://localhost:1880
You can access the Grafana page at this address:
http://localhost:3000/

The username/password are "**admin**"

# 4. What is missing (bugs)

In this project, some parts are missing in order to finish it.

1. **(BUG)** When we take a photo with the interface on the web server, the photo is correctly displayed wwith the cropped pictures. However, the pictures saved on the microSD are ALWAYS the same.

Here is the picture I take with the CAM

![alt text for screen readers](/picture_ReadMe/WebServer.png "WebServer pictures")

Here is the picture saved on the microSD

![alt text for screen readers](/picture_ReadMe/MicroSD.png "MicroSD pictures")

I attempted to delete these pictures from the microSD, but they keep reappearing. I’ve formatted the microSD and cleared all memory allocations made in the Arduino code, yet the same pictures persist. They might be stored somewhere that I can’t locate.

2. **(Missing part)** Without the previous point, I was not able to test the CNN, so it is missing in the arduino code. He is converted in C code and uploaded on the github project. File named **"cnnFile.h"**.
3. **(Missing part)** Last part is the LoRaWAN part. It is needed to code the data transmission to LoRaWAN gateway.
4. **(Potential problem)** You may have a problem with the prediction because the pictures that the ESP32-CAM take have a really bad quality in my opinion. Even a human struggle to predict the value on the picture.

