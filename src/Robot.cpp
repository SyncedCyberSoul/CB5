#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <WPILib.h>
#include <CameraServer.h>
#include <unistd.h>
#include <IterativeRobot.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/core/types.hpp>        //all of these might not be necessary


class Robot: public frc::IterativeRobot {
public:
	int    ii, jj, stripe_width, num_rows, num_columns, stripe_start_row, diff_int[641], integral[641], timesThrough;					// Vision
	int    max_integral, maxposn1, maxposn2, min_integral, minposn1, minposn2, tempi, cutoff_intensity, flagi, done_int, targetCenter;	//
	int    max_intensity_cutoff, max_cutoff_picture;																					//
	double bias, tempf, centerField, kdiff;																								//
	double extra, scale, turnback;																										//
	bool   greenholder, validView;																										//

	bool   leg0, leg1, leg2,wallhit=0;																											// Auto
	bool   gearDrop, backUp, bumper, visionOn;																							//
	double anglestart, angleend, P_differential;																						//
	int    turnside, posn_previous;																													//

	int    arm_max=355, arm_set_up=70, arm_set_down=346;																				// Gear
	bool   kickerdown, kickerup, kickerEreset;																							//
	bool   kickerdummy, kickerrunning, stop_arm1;																						//

	double targetShotSpeed, kp, kint, speedNow, error, errorInt, percentI,shotspeed, kp_drivestraight, encerror;						//Shooter and PIDs
	bool   Bobby, BobbyB;																												//

	double climbspeed, heading, calibrator=883.95/12.0/*ticks per foot*/;																// Movement
	double Leftgo, Rightgo, Rdis, Ldis;																									//

	bool   superdum,lightdum=1, disable_DrC=0;																							// Misc.
	int count=1;																														//

	Joystick *rightDrive =new Joystick(0,2,9);
	Joystick *leftDrive  =new Joystick(1,2,9);
	Joystick *gamePad    =new Joystick(2,6,9);

	Spark *fLeft         =new Spark(0);
	Spark *fRight        =new Spark(1);
	Spark *bLeft         =new Spark(2);
	Spark *bRight        =new Spark(3);
	Spark *kicker        =new Spark(4);
	Spark *climber       =new Spark(5);
	Spark *frankenspark  =new Spark(6);
	Spark *shooter		 =new Spark(7);
	Spark *feeder		 =new Spark(8);

	Encoder *encRight    =new Encoder(0,1);
	Encoder *encLeft     =new Encoder(2,3);
	Encoder *encKicker	 =new Encoder(4,5);
	Encoder *encShooter  =new Encoder(6,7);
	DigitalInput *limitArm   = new DigitalInput(8);    //reads the arm limit switch
	DigitalInput *bumperHit  = new DigitalInput(9);   //the bumper is hit.

	frc::ADXRS450_Gyro *gyro = new frc::ADXRS450_Gyro(frc::SPI::kOnboardCS0);
	RobotDrive *robotDrive   = new RobotDrive(fLeft,bLeft,fRight,bRight);
	//Auto Camera
	cs::UsbCamera cam2		= CameraServer::GetInstance()->StartAutomaticCapture(1);//sets up camera 2 for capturing
	//Auto Camera
	//Matrixes
	cv::Mat pregreen 	= cv::Mat(640,480,CV_8U);
	cv::Mat green 		= cv::Mat(640,480,CV_8U);
	std::vector<cv::Mat> planes;
	//Matrixes

	void RobotInit() {
		//std::thread camthread(VisionThread);//makes a new thread
		//camthread.detach();//snaps the thread off to do its own thing

		cam2.SetBrightness(1200);
		cam2.SetExposureManual(42);
		cam2.SetWhiteBalanceManual(3800);
		cam2.SetResolution(640,480);


		chooser.AddDefault(NOTHING, NOTHING);
		chooser.AddObject(DOA, DOA);
		chooser.AddObject(FORWARD, FORWARD);
		//chooser.AddObject(FCOMP, FCOMP);
		chooser.AddObject(lefthook, lefthook);
		chooser.AddObject(righthook, righthook);
		frc::SmartDashboard::PutData("Auto Modes", &chooser);

		encRight->Reset();
		encLeft->Reset();
		gyro->Calibrate();
	}

	/*
	 * This autonomous (along with the chooser code above) shows how to select
	 * between different autonomous modes using the dashboard. The sendable
	 * chooser code works with the Java SmartDashboard. If you prefer the
	 * LabVIEW Dashboard, remove all of the chooser code and uncomment the
	 * GetString line to get the auto name from the text box below the Gyro.
	 *
	 * You can add additional auto modes by adding additional comparisons to the
	 * if-else structure below with additional strings. If using the
	 * SendableChooser make sure to add them to the chooser code above as well.
	 */

	void AutonomousInit() override {
		autoSelected = chooser.GetSelected();
		std::cout << "Auto selected: " << autoSelected << std::endl;
		stop_arm1=limitArm->Get();
		while(!stop_arm1&&!IsOperatorControl()){
			stop_arm1=limitArm->Get();
			kicker->Set(.5);
		}
		kicker->Set(0);
		encKicker->Reset();
		encRight->Reset();
		encLeft->Reset();
		greenholder=0;
		gearDrop=0;
		kickerdummy=0;
		backUp=0;
		leg0=0;
		leg1=0;
		leg2=0;
		visionOn=0;
		Rightgo=0;
		Leftgo=0;
		scale = 1.0;
		turnback = 1.0;
		Rdis=0;
		Ldis=0;
		kp_drivestraight = .1/10000;
	}

	//AUTO START

	void AutonomousPeriodic() {
		Rdis=encRight->GetRaw();
		Ldis=encLeft->GetRaw();
		extra = (Ldis-Rdis)*scale;
		bumper=bumperHit->Get();
		count++;
		if(count%200==0&&Leftgo>0&&Rightgo>0&&fabs(posn_previous-(Ldis+Rdis))<=200){
			wallhit=1;
		}
		else{
			wallhit=0;
			posn_previous=(Ldis+Rdis);
		}
		//GLOBAL GEAR
		if(gearDrop==1){//call gearDrop to drop gear
			if((!backUp)&&(!kickerdummy)){//Kick gear
				kicker->Set(.5*((encKicker->GetRaw())-arm_set_down)/arm_max-0.7);//Move Forwards PID
				if((encKicker->GetRaw())>=arm_set_down){
					kickerrunning=0; // no longer running
					kickerdummy=1;
					kicker->Set(0);
					encRight->Reset();
					encLeft->Reset();
				}
			}
			else if(!backUp){
				sleep(1.0);
				Rightgo=-.75;
				Leftgo=-.75;
				if(abs(encRight->GetRaw())>=550){//Slide back a little
					Rightgo=0;
					Leftgo=0;
					backUp=1;
				}
			}
/*			else if(kickerdummy){//Pull arm back up
				kicker->Set(.95*((encKicker->GetRaw())-arm_set_up)/arm_max+0.1);//Move Backwards PID and slows down
				if((encKicker->GetRaw())<=arm_set_up){
					kickerdummy=0;
					kicker->Set(0);
				}
			}*/
			else{//Gear loop done
				Rightgo=0;
				Leftgo=0;
			}
		}
		//GLOBAL GEAR


		//GLOBAL VISION
		/*
		if(visionOn){		//computer vision detect the strips for driving/only for forwardlight, lefthook, righthook
			if(!greenholder){
				autosinker.GrabFrame(pregreen);//grabs a pregreen image
				frankenspark->Set(-1);//turn on the lights
				sleep(1.0);//1 sec delay for light to turn on
				autosinker.GrabFrame(green);//grabs a green image
				frankenspark->Set(0);//turn off the lights THERE TO BRIGHT!
				greenholder=1;
			}
			//
			else if(greenholder){
				cv::addWeighted(green,1,pregreen,-1,0,green);//meshes pregreen and green then outputs to green Needs to be values of 1
				split(green,planes); //splits Matrix green into 3 BGR planes called planes

				max_cutoff_picture = 50;
				cv::threshold(planes[1],planes[1],max_cutoff_picture, 0 ,cv::THRESH_TOZERO);//anything less then cutoff is set to zero everything else remains the same

				// Dr. C.'s codelines for integrated luminosity lines
				num_columns = 640;                    // the x-and y- number of pixels.
				num_rows = 480;                       //
				stripe_start_row = 120;               // start row for the strip to take
				stripe_width = 40;                    // pixel width of the strip
				max_integral = -255*stripe_width;     // min and max values to start the
				min_integral = 255*stripe_width;      //
				cutoff_intensity = 3*stripe_width;    // saturation dark value on sum
				max_intensity_cutoff = 38;           // saturation light value on pixels
				bias = 0.35*255*stripe_width/num_columns; // A bias to weight the max and min positions
				targetCenter = 320;
				kdiff = 1.0/320.0;
				//Intensity Catch
				for(ii=2; ii<num_columns; ii++){//gets x values
					integral[ii] = 0; //Initialize as zero

					for(jj=stripe_start_row; jj<stripe_start_row+stripe_width; jj++){//Gets y values
						tempi = (int)((planes[1]).at<uchar>(jj,ii));//grabs intensity values
						if (tempi>max_intensity_cutoff){//Regulate the pixel intensities
							tempi = 255;
						}
						integral[ii] = integral[ii]+tempi;//adds all of the pixels in the columns
					}

					if (integral[ii]<cutoff_intensity){//checks if in the blank spaces there is a little bit of noise
						integral[ii] = 0;      //assume that we reach zero somewhere in between the stripes
					}

					diff_int[ii] = integral[ii]-integral[ii-1]; // find the boundaries
				}
				//Intensity Catch

				// 'bias' breaks the degeneracy between the maxs and mins
				max_integral = -255*stripe_width;                     // min and max values to start the
				min_integral = 255*stripe_width;

				//Stripe 1
				for(ii=2; ii<num_columns; ii++){
					tempi = diff_int[ii];      //temporary integer; speeds up lookup in next lines.
					if(max_integral<tempi+(int)(ii*bias)){    //if new value exceeds old, max set to new max
						maxposn1 = ii;         //gets value of global max
						max_integral=tempi+(int)(ii*bias);
					}
					if(min_integral>tempi-(int)(ii*bias)){    //if new value exceeds old min, set to new global min
						minposn1 = ii;
						min_integral=tempi-(int)(ii*bias);
					}
				}
				//Stripe 1

				//Stripe 2
				bias = -bias;//flip the bias to find the second positions
				max_integral = -255*stripe_width;             //Reset the max and min
				min_integral = 255*stripe_width;
				for(ii=minposn1+2; ii<num_columns; ii++){
					tempi = diff_int[ii];                     //temporary integer; speeds up lookup in next lines.
					if(max_integral<tempi+(int)(ii*bias)){    //if new value exceeds old, max set to new max
						maxposn2 = ii;                        //gets value of global max
						max_integral=tempi+(int)(ii*bias);
					}
					if(min_integral>tempi-(int)(ii*bias)){    //if new value exceeds old min, set to new global min
						minposn2 = ii;
						min_integral=tempi-(int)(ii*bias);
					}
				}
				//Stripe 2

				//Position Check
				if(maxposn2>maxposn1&&diff_int[maxposn2]>5){
					done_int=1;//Its good!
				}
				else{
					done_int=0;//Not so good
				}
				//Position Check

				//Recount
				if(done_int==0){//If the check fails try recounting 2
					max_integral = -255*stripe_width;             // Reset values
					min_integral = 255*stripe_width;

					for(ii=2; ii<maxposn1; ii++){// Recounts for second stripe up to maxposn1
						tempi = diff_int[ii];                     //temporary integer; speeds up lookup in next lines.
						if(max_integral<tempi+(int)(ii*bias)){    //if new value exceeds old, max set to new max
							maxposn2 = ii;                        //gets value of global max
							max_integral=tempi+(int)(ii*bias);
						}
						if(min_integral>tempi-(int)(ii*bias)){    //if new value exceeds old min, set to new global min
							minposn2 = ii;
							min_integral=tempi-(int)(ii*bias);
						}
					}

					tempi = maxposn2;		//These lines flip the max and min positions
					maxposn2 = maxposn1;
					maxposn1 = tempi;
					tempi = minposn2;
					minposn2 = minposn1;
					minposn1 = tempi;
					done_int = 1;//Its good now
				}
				//Recount

				// at this point should be all done. Can check done_int=1 and if good you should have the ordered set
				//    (maxposn1, minposn1, maxposn2, minposn2) of the pixel numbers of the stripe edges!!

				// Validate Data
				validView=0;
				tempf=abs((maxposn1-minposn1)/(maxposn2-minposn2));
				if(done_int==1) {
					if((maxposn2>num_columns-4)&&(minposn1<4)){     // check that the bands are inside the FOV...and not grabbing an edge
						validView=0;
					}
					else if((tempf<.8)||(tempf>1.0/0.8)){                 // check that the bands you find are roughly the same size in pixels
						validView=0;
					}
					else if (abs(minposn1-maxposn2)<4){					 // check not concatenated boxes
						validView=0;
					}
					else if((abs(minposn1-minposn2)<4)||(abs(maxposn1-maxposn2)<4)){ // check not interpenetrating boxes.
						validView=0;
					}
					else{
						validView=1;//If all the tests fail it must be good image read
					}
				}
				centerField =(maxposn1+minposn1+maxposn2+minposn2)/4.0;
				greenholder=0;
				visionOn=0;
			}
		}
		*/
		//GLOBAL VISION

		/*
		 *
		 *
		 *
		 * Leaving Global Auto
		 *
		 *
		 *
		 */

		//DOA
		if(autoSelected == DOA){//Dead On Arrival AKA Dead Reckoning

			if(!gearDrop&&fabs(Rdis)<=5117.67&&fabs(Ldis)<=5117.67&&!wallhit){//114.3" from wall to wall of airship ~6.92 feet
				Rightgo=.75;
				Leftgo=.80;
			}
			else if(!gearDrop){
				Rightgo=0;
				Leftgo=0;
				gearDrop=1;
			}
		}
		//DOA end

		//FORWARD
		/*else if(autoSelected == FORWARD){

			if(fabs(Rdis)<=7117.67&&fabs(Ldis)<=7117.67){//114.3" from wall to wall of airship ~6.92 feet
				Rightgo=.75;
				Leftgo=.80;

			}
			else{
				Rightgo=0;
				Leftgo=0;
			}
		}*/
		// simplest DrC hook!
		else if (((autoSelected==righthook)||(autoSelected==lefthook))&&disable_DrC==0) {
							// HOOK SELECTION
							// go forward 88.09 inches, turn 60 degrees and then go 29.22 inches.
							if (autoSelected == lefthook) {
								turnside = -1;
							}
							else if (autoSelected == righthook) { // Fixed so the Auto does not fall through
								turnside = 1;
							}
				// LEG 0 : got forward
				if(!gearDrop&&fabs(Rdis)<=88.09*calibrator&&fabs(Ldis)<=88.09*calibrator&&leg0==0){
					Rightgo=fabs(.75);
					Leftgo =fabs(.75);
				}
				if(!gearDrop&&fabs(Rdis)>=88.09*calibrator&&fabs(Ldis)>=88.09*calibrator&&leg0==0){
					Rightgo= 0.0;
					Leftgo = 0.0;
					leg0=1;
					encRight->Reset();
					encLeft->Reset();
				}
				// LEG 1 : rotate
				if(!gearDrop&&(fabs(extra)<=13.22/2.0*calibrator)&&leg0==1&&leg1==0){      // LEG 1: then turn, 13.22 inches (60 degrees) to normal to face of airship wall
					Rightgo=+.25*turnside;
					Leftgo=-.25*turnside;
					}
				if(!gearDrop&&(fabs(extra)>=13.22/2.0*calibrator)&&leg0==1&&leg1==0){
					Rightgo= 0.0;
					Leftgo = 0.0;
					leg1=1;
					encRight->Reset();
					encLeft->Reset();
				}
				//LEG 3 : drive forward and drop gear
				if(!gearDrop&&fabs(Rdis)<=29.22*calibrator&&fabs(Ldis)<=29.22*calibrator&&leg0==1&&leg1==1&&leg2==0){
						Rightgo=fabs(.75);
						Leftgo =fabs(.75);
									}
				if(!gearDrop&&fabs(Rdis)>=29.22*calibrator&&fabs(Ldis)<=29.22*calibrator&&leg0==1&&leg1==1&&leg2==0){
						Rightgo= 0.0;
						Leftgo = 0.0;
						leg2=1;
						encRight->Reset();
						encLeft->Reset();
						gearDrop=1;
				}
		}
		/*
		if(!disable_DrC){//Dr. C Kill switch
			// HOOK
			if ((autoSelected == righthook)||(autoSelected == lefthook)) {

				// HOOK SELECTION
				// go forward 88.09 inches, turn 60 degrees and then go 29.22 inches.
				if (autoSelected == lefthook) {
					turnside = -1;
				}

				else if (autoSelected == righthook) { // Fixed so the Auto does not fall through
					turnside = 1;
				}
				//HOOK SELECTION
				// LEG 0
				if(!gearDrop&&Rdis<=88.09*calibrator&&Ldis<=88.09*calibrator&&leg0==0){
					Rightgo=fabs(.75+extra);
					Leftgo =fabs(.75-extra);
				}
				else{
					leg0=1;                               // END OF LEG 0: signal you are done with first straightaway.
					encRight->Reset();
					encLeft->Reset();
					anglestart = gyro->GetAngle();
				}
				if(!gearDrop&&leg0==1&&leg1==0){      // LEG 1: then turn, 13.22 inches (60 degrees) to normal to face of airship wall
					angleend = gyro->GetAngle();
					if ( ((((int)(angleend-anglestart-turnside*60) % 360)>2)||(((int)(angleend-anglestart-turnside*60) % 360)<360-2))&&(abs(Rdis)<60*calibrator)&&(fabs(1.0-fabs(extra)/scale/13.22/calibrator)>.03)) {// this is the angular tolerance to get you within 2-3 degrees of normal to the gear pin.
						if (1.0-fabs(extra)/scale/13.22/calibrator<0) {     // Only if both are beyond the target, turn in the opposite direction!
							turnback = -1.0;
						}
						else{
							turnback = 1.0;
						}
						visionOn=1;                                // turn on vision capture.
						if(!bumper&&validView){					   //here decide the angle to drive
							P_differential = kdiff*(targetCenter-centerField);
							visionOn=0;                            // turn the vision off.
							validView=0;
							turnback = 1.0;                        // if have a valid view, drive with the camera
							turnside = 1.0;
						}
						else{
							P_differential = 1.0;                  // else drive with the turnside/turnback combo only.
						}
						Rightgo=+.25*turnside*turnback*P_differential;
						Leftgo=-.25*turnside*turnback*P_differential;
					}
					else {
						Rightgo= 0.0;
						Leftgo = 0.0;
						leg1=1;
						encRight->Reset();
						encLeft->Reset();
						timesThrough=1;
					}
				}
				if(!gearDrop&&leg0==1&&leg1==1&&leg2==0){      // LEG 2: then crawl onto the peg, with the robot vision.
					if(Rdis<=29.22/3.0*timesThrough*calibrator&&Ldis<=29.22/3.0*timesThrough*calibrator&&!bumper){
						Rightgo=fabs(.75+extra);
						Leftgo =fabs(.75-extra);
					}
					else{
						Rightgo= 0.0;
						Leftgo = 0.0;
						leg2=1;
						encRight->Reset();
						encLeft->Reset();
					}
					if(bumper||(timesThrough==4)){
						leg0=1;
						leg1=1;
						leg2=1;
						Rightgo= 0.0;
						Leftgo = 0.0;
						encRight->Reset();
						encLeft->Reset();
						gearDrop=1;
						visionOn=0;
					}
				}
			}
			//HOOK end
		}//Dr. C Kill switch
		 */
		else if(autoSelected == FORWARD){
					encerror=(Ldis-Rdis);
					if(fabs(Rdis)<=7117.67&&fabs(Ldis)<=7117.67){//114.3" from wall to wall of airship ~6.92 feet
						Rightgo=.75+kp_drivestraight*encerror;
						Leftgo=.75-kp_drivestraight*encerror;
						if(Rightgo>1){
							Rightgo=1;
						}
						if(Leftgo>1){
							Leftgo=1;
						}

					}
					else{
						Rightgo=0;
						Leftgo=0;
					}
				}
		//NOTHING
		else if (autoSelected == NOTHING) {//sit there yah lazy bum
			Leftgo=0;
			Rightgo=0;
		}
		//NOTHING end

		robotDrive->TankDrive(Leftgo,Rightgo);
		SmartDashboard::PutNumber("encRight",Rdis);
		SmartDashboard::PutNumber("encLeft", Ldis);
	}
	//AUTO END


	void TeleopInt() {
		Leftgo      =0;
		Rightgo     =0;
		encRight->Reset();
		encLeft->Reset();
		targetShotSpeed = 8.5;          // target for the shooter wheel speed
		kp = .4/10000;//.4/10000;                               // proportional gain for the PID loop for shooter wheel
		kint = 1.7/10000;//1.7/10000;						     // integral gain for PID loop         for shooter wheel
		percentI = .84;  							 // integrating time-to-forget
	}
	//TELE START
	void TeleopPeriodic() {
		//Video Practice
		superdum=gamePad->GetRawButton(7);
		greenholder=0;
		/*while(superdum){
			if(!greenholder){
				autosinker.GrabFrame(pregreen);//grabs a pregreen image
				frankenspark->Set(-1);//turn on the lights
				sleep(1.0);//1 sec delay for light to turn on
				autosinker.GrabFrame(green);//grabs a green image
				frankenspark->Set(0);//turn off the lights THERE TO BRIGHT!
				greenholder=1;
			}
			//
			else if(greenholder){
				cv::addWeighted(green,1,pregreen,-1,0,green);//meshes pregreen and green then outputs to green Needs to be values of 1
				split(green,planes); //splits Matrix green into 3 BGR planes called planes

				max_cutoff_picture = 50;
				cv::threshold(planes[1],planes[1],max_cutoff_picture, 0 ,cv::THRESH_TOZERO);//anything less then cutoff is set to zero everything else remains the same

				// Dr. C.'s codelines for integrated luminosity lines
				num_columns = 640;                    // the x-and y- number of pixels.
				num_rows = 480;                       //
				stripe_start_row = 120;               // start row for the strip to take
				stripe_width = 40;                    // pixel width of the strip
				max_integral = -255*stripe_width;     // min and max values to start the
				min_integral = 255*stripe_width;      //
				cutoff_intensity = 3*stripe_width;    // saturation dark value on sum
				max_intensity_cutoff = 38;           // saturation light value on pixels
				bias = 0.35*255*stripe_width/num_columns; // A bias to weight the max and min positions
				targetCenter = 320;

				//Intensity Catch
				for(ii=2; ii<num_columns; ii++){//gets x values
					integral[ii] = 0; //initilize as zero

					for(jj=stripe_start_row; jj<stripe_start_row+stripe_width; jj++){//Gets y values
						tempi = (int)((planes[1]).at<uchar>(jj,ii));//grabs intensity values
						if (tempi>max_intensity_cutoff){//Regulate the pixel intensitys
							tempi = 255;
						}
						integral[ii] = integral[ii]+tempi;//adds all of the pixels in the columns
					}

					if (integral[ii]<cutoff_intensity){//checks if in the blank spaces there is a little bit of noise
						integral[ii] = 0;      //assume that we reach zero somewhere in between the stripes
					}

					diff_int[ii] = integral[ii]-integral[ii-1]; // find the boundaries
				}
				//Intensity Catch

				// 'bias' breaks the degeneracy between the maxs and mins
				max_integral = -255*stripe_width;                     // min and max values to start the
				min_integral = 255*stripe_width;

				//Stripe 1
				for(ii=2; ii<num_columns; ii++){
					tempi = diff_int[ii];      //temporary integer; speeds up lookup in next lines.
					if(max_integral<tempi+(int)(ii*bias)){    //if new value exceeds old, max set to new max
						maxposn1 = ii;         //gets value of global max
						max_integral=tempi+(int)(ii*bias);
					}
					if(min_integral>tempi-(int)(ii*bias)){    //if new value exceeds old min, set to new global min
						minposn1 = ii;
						min_integral=tempi-(int)(ii*bias);
					}
				}
				//Stripe 1

				//Stripe 2
				bias = -bias;//flip the bias to find the second positions
				max_integral = -255*stripe_width;             //Reset the max and min
				min_integral = 255*stripe_width;
				for(ii=minposn1+2; ii<num_columns; ii++){
					tempi = diff_int[ii];                     //temporary integer; speeds up lookup in next lines.
					if(max_integral<tempi+(int)(ii*bias)){    //if new value exceeds old, max set to new max
						maxposn2 = ii;                        //gets value of global max
						max_integral=tempi+(int)(ii*bias);
					}
					if(min_integral>tempi-(int)(ii*bias)){    //if new value exceeds old min, set to new global min
						minposn2 = ii;
						min_integral=tempi-(int)(ii*bias);
					}
				}
				//Stripe 2

				//Position Check
				if(maxposn2>maxposn1&&diff_int[maxposn2]>5){
					done_int=1;//Its good!
				}
				else{
					done_int=0;//Not so good
				}
				//Position Check

				//Recount
				if(done_int==0){//If the check fails try recounting 2
					max_integral = -255*stripe_width;             // Reset values
					min_integral = 255*stripe_width;

					for(ii=2; ii<maxposn1; ii++){// Recounts for second stripe up to maxposn1
						tempi = diff_int[ii];                     //temporary integer; speeds up lookup in next lines.
						if(max_integral<tempi+(int)(ii*bias)){    //if new value exceeds old, max set to new max
							maxposn2 = ii;                        //gets value of global max
							max_integral=tempi+(int)(ii*bias);
						}
						if(min_integral>tempi-(int)(ii*bias)){    //if new value exceeds old min, set to new global min
							minposn2 = ii;
							min_integral=tempi-(int)(ii*bias);
						}
					}

					tempi = maxposn2;		//These lines flip the max and min positions
					maxposn2 = maxposn1;
					maxposn1 = tempi;
					tempi = minposn2;
					minposn2 = minposn1;
					minposn1 = tempi;
					done_int = 1;//Its good now
				}
				//Recount

				// at this point should be all done. Can check done_int=1 and if good you should have the ordered set
				//    (maxposn1, minposn1, maxposn2, minposn2) of the pixel numbers of the stripe edges!!

				// Validate Data
				validView=0;
				tempf=abs((maxposn1-minposn1)/(maxposn2-minposn2));
				if(done_int==1) {
					if((maxposn2>num_columns-4)&&(minposn1<4)){     // check that the bands are inside the FOV...and not grabbing an edge
						validView=0;
					}
					else if((tempf<.8)||(tempf>1.0/0.8)){                 // check that the bands you find are roughly the same size in pixels
						validView=0;
					}
					else if (abs(minposn1-maxposn2)<4){					 // check not concatenated boxes
						validView=0;
					}
					else if((abs(minposn1-minposn2)<4)||(abs(maxposn1-maxposn2)<4)){ // check not interpenetrating boxes.
						validView=0;
					}
					else{
						validView=1;//If all the tests fail it must be good image read
					}
				}
				centerField =(maxposn1+minposn1+maxposn2+minposn2)/4.0;
				greenholder=0;
				SmartDashboard::PutNumber("camera first stripe outer edge", maxposn1);
				SmartDashboard::PutNumber("Camera first stripe inner edge", minposn1);
				SmartDashboard::PutNumber("diff max", diff_int[maxposn1]);
				SmartDashboard::PutNumber("diff inner", diff_int[minposn1]);
				SmartDashboard::PutNumber("intergral at max",integral[maxposn1]);
				SmartDashboard::PutNumber("intergral at max-1",integral[maxposn1-1]);
				SmartDashboard::PutNumber("camera second stripe inner edge", maxposn2);
				SmartDashboard::PutNumber("Camera second stripe outer edge", minposn2);
				SmartDashboard::PutBoolean("Valid", validView);
				SmartDashboard::PutNumber("Center of Field",centerField);
				rectangle(planes[1], cv::Point(maxposn1, 120), cv::Point(minposn1, 160),cv::Scalar(255, 255, 255), 5);//first stripe
				rectangle(planes[1], cv::Point(maxposn2, 120), cv::Point(minposn2, 160),cv::Scalar(255, 0, 0), 2);//second stripe
				rectangle(planes[1], cv::Point((maxposn1+minposn1)/2, 128), cv::Point((maxposn2+minposn2)/2, 132),cv::Scalar(255, 0, 0), 6);//second stripe
				camserver.PutFrame(planes[1]);
				superdum=0;
			}
		}
		//Video Practice
		 */
		if(gamePad->GetRawButton(3)){

			frankenspark->Set(-1);
		}
		else{
			frankenspark->Set(0);
		}
		//Drive
		Leftgo =-1*leftDrive->GetRawAxis(1);
		Rightgo=-1*rightDrive->GetRawAxis(1);
		Rdis=encRight->GetRaw();
		Ldis=encLeft->GetRaw();
		robotDrive->TankDrive(Leftgo,Rightgo);
		//Drive

		//Kicker
		stop_arm1=limitArm->Get();
		if(!kickerEreset){
			kickerEreset=rightDrive->GetRawButton(3);
		}
		if(kickerEreset){
			if(!stop_arm1){
				kicker->Set(.25);
			}
			else{
				kicker->Set(0);
				encKicker->Reset();
				kickerrunning=0;
				kickerdummy=0;
				kickerEreset=0;
			}
		}
		if(!kickerEreset){
			if(!kickerrunning){
				kickerdown   =rightDrive->GetRawButton(1);
				kickerup   =leftDrive->GetRawButton(1);
			}

			if(kickerdown&&!kickerdummy){//Forward
				kickerrunning=1;
				kicker->Set(.5*((encKicker->GetRaw())-arm_set_down)/arm_max-0.5);//Move Forwards PID
				if((encKicker->GetRaw())>=arm_set_down){
					kickerrunning=0;//No Longer Running
					kickerdummy=1;
					kicker->StopMotor();
				}
			}
			else if(kickerup&&kickerdummy){//Reverse
				kickerrunning=1;
				kicker->Set(.95*((encKicker->GetRaw())-arm_set_up)/arm_max+0.3);//Move Backwards PID and slows dows
				if((encKicker->GetRaw())<=arm_set_up){//Stop Early to Comp for Drift
					kickerrunning=0;//No Longer Running
					kickerdummy=0;
					kicker->Set(0);
				}
			}
			else{//Stop if no button
				kicker->Set(0);
			}
		}
		SmartDashboard::PutNumber("enckicker",encKicker->GetRaw());
		//Kicker

		//Shooter
		shotspeed= (gamePad->GetRawAxis(3)) - (gamePad->GetRawAxis(2));
		speedNow = -(encShooter->GetRate());
		if(shotspeed>=.5){
			error = speedNow-10000*targetShotSpeed;
			errorInt = percentI*errorInt+(1.0-percentI)*error;
			shooter->Set(-.58+(kp*error)+(kint*errorInt));
		}
		else if(shotspeed<=-.5){
			shooter->Set(.5);

		}
		else{
			shooter->Set(0);
			error=0;
			errorInt=0;
		}
		SmartDashboard::PutNumber("shotspeed",shotspeed);
		SmartDashboard::PutNumber("encoder integrated error", errorInt);
		SmartDashboard::PutNumber("error",error);
		SmartDashboard::PutNumber("Speed Now!!!",speedNow);
		SmartDashboard::PutNumber("Speed error", fabs(speedNow/(targetShotSpeed*10000)));
		SmartDashboard::PutNumber("Polyphemous is a wimp!",encShooter->GetRaw());
		if(fabs(speedNow)>=85000 && fabs(speedNow)<=90000){
			gamePad->SetRumble(Joystick::RumbleType::kRightRumble,1);
			gamePad->SetRumble(Joystick::RumbleType::kLeftRumble,1);
		}
		else{
			gamePad->SetRumble(Joystick::RumbleType::kRightRumble,0);
			gamePad->SetRumble(Joystick::RumbleType::kLeftRumble,0);
		}
		//Shooter

		// Bobby
		Bobby  = gamePad -> GetRawButton(5);
		BobbyB = gamePad -> GetRawButton(6);
		if(Bobby==1){
			feeder->Set(0.75);
		}
		else if (BobbyB==1){
			feeder->Set(-0.75);
		}
		else {
			feeder ->Set(0);
		}
		//Bobby

		//Climber
		climbspeed=gamePad->GetRawAxis(0);
		if(fabs(climbspeed)>=.25){
			climber->Set(-fabs(climbspeed));
		}
		else{
			climber->Set(0);
		}
		//Climber

		heading = gyro->GetAngle();

		//SmartDashboard
		SmartDashboard::PutNumber("Heading", heading);
		SmartDashboard::PutBoolean("Bumber switch",bumperHit->Get());
		SmartDashboard::PutNumber("encRight",Rdis);
		SmartDashboard::PutNumber("encLeft", Ldis);
		SmartDashboard::PutNumber("limitArm", stop_arm1);
		SmartDashboard::PutNumber("Leftgo",Leftgo);
		SmartDashboard::PutNumber("Rightgo",Rightgo);
		//SmartDashboard
	}

	//TELE END

	void TestPeriodic() {
		lw->Run();
	}

private://why is this down here?
	frc::LiveWindow* lw = LiveWindow::GetInstance();
	frc::SendableChooser<std::string> chooser;
	const std::string NOTHING  =  "NOTHING!";
	const std::string DOA = "Dead Reckoning Straight";
	const std::string FORWARD = "Just Forward";
	const std::string lefthook  = "Left Hook";
	const std::string righthook = "Right Hook";
	std::string autoSelected;
};

START_ROBOT_CLASS(Robot)

/* Hardware map of the robot "TBA"  (CB5)
 *	1ft=883.95 Wheel Encoders
 *
 *		RRio Pins
 * 		PWM
 *		0 Front Left
 *		1 	"	Right
 *		2 Back	Left
 *		3	"	Right
 *		4 Kicker
 *		5 Climber
 *		6 Franken Spark: green light + Under-glow
 *		7 Shooter
 *		8 Feeder
 *		9
 *
 *
 *  	DIO
 *  	0	A Right Wheel Encoder   (Blue wire)
 *  	1	B  "        (Yellow Wire)
 *  	2	A Left Wheel Encoder (Blue Wire)
 *  	3	B  "   		(Yellow Wire)
 *  	4	A Kicker Encoder
 *  	5	B  "
 *  	6 	A Shooter Encoder (Blue Wire)
 *  	7   B  "		(Yellow Wire)
 *  	8   Limit Switch (kicker arm) for the encoder calibration (registration mark)
 *  	9   Bumper Contact switch
 *  	10	Not currently used (pins 11 and 12 in mxp port)
 *
 *
 *  	Analog
 *  	0
 *  	1
 *  	2
 *  	3
 *
 *		Relay
 *		0
 *		1
 *		2
 *		3
 *
 *
 *
 *
 *
 *
 */
