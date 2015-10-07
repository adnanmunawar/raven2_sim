/* Raven 2 Control - Control software for the Raven II robot
 * Copyright (C) 2005-2012  H. Hawkeye King, Blake Hannaford, and the University of Washington BioRobotics Laboratory
 *
 * This file is part of Raven 2 Control.
 *
 * Raven 2 Control is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Raven 2 Control is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Raven 2 Control.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
*  \file fwd_cable_coupling.cpp
*  \brief Calculate the forward cable coupling from a motor space Pose,
*         (m1, m2,m3) express the desired joint pose (th1, th2, d3)
*  \author Hawkeye
*/

#include "fwd_cable_coupling.h"
#include "log.h"
#include "tool.h"
#include "two_arm_dyn.h"
#include <time.h>

extern struct DOF_type DOF_types[];
extern int NUM_MECH;
extern long int iteration;
extern unsigned long int gTime;

state_type r_d_gold, r_d_green;
struct timespec t1, t2; 

/**
* \fn void fwdCableCoupling(struct device *device0, int runlevel)
* \brief Calls fwdMechCableCoupling for each mechanism in device
* \param device0 - pointer to device struct
* \param runlevel - current runlevel
* \return void
* \todo Remove runlevel from args.
*/

void fwdCableCoupling(struct device *device0, int runlevel)
{
	//Run fwd cable coupling for each mechanism.
	// This should be run in all runlevels.
	for (int i = 0; i < NUM_MECH; i++)
		fwdMechCableCoupling(&(device0->mech[i]));
#ifdef simulator
        //log_file("RT_PROCESS) FWD Cable Coupling Done");         
#endif
}

/**
* \fn void fwdMechCableCoupling(struct mechanism *mech)
* \param mech
* \return void
*/

void fwdMechCableCoupling(struct mechanism *mech)
{
	float th1, th2, th3, th5, th6, th7;
	float th1_dot, th2_dot;
	float d4;
	float d4_dot;
	float m1, m2, m3, m4, m5, m6, m7;
	float m1_dot, m2_dot, m4_dot;
	float tr1=0, tr2=0, tr3=0, tr4=0, tr5=0, tr6=0, tr7=0;

	m1 = mech->joint[SHOULDER].mpos;
	m2 = mech->joint[ELBOW].mpos;
	m3 = mech->joint[TOOL_ROT].mpos;
	m4 = mech->joint[Z_INS].mpos;
	m5 = mech->joint[WRIST].mpos;
	m6 = mech->joint[GRASP1].mpos;
	m7 = mech->joint[GRASP2].mpos;

	m1_dot = mech->joint[SHOULDER].mvel;
	m2_dot = mech->joint[ELBOW].mvel;
	m4_dot = mech->joint[Z_INS].mvel;

	if (mech->type == GOLD_ARM){
		tr1=DOF_types[SHOULDER_GOLD].TR;
		tr2=DOF_types[ELBOW_GOLD].TR;
		tr3=DOF_types[TOOL_ROT_GOLD].TR;
		tr4=DOF_types[Z_INS_GOLD].TR;
		tr5=DOF_types[WRIST_GOLD].TR;
		tr6=DOF_types[GRASP1_GOLD].TR;
		tr7=DOF_types[GRASP2_GOLD].TR;

	} else if (mech->type == GREEN_ARM){
		tr1=DOF_types[SHOULDER_GREEN].TR;
		tr2=DOF_types[ELBOW_GREEN].TR;
		tr3=DOF_types[TOOL_ROT_GREEN].TR;
		tr4=DOF_types[Z_INS_GREEN].TR;
		tr5=DOF_types[WRIST_GREEN].TR;
		tr6=DOF_types[GRASP1_GREEN].TR;
		tr7=DOF_types[GRASP2_GREEN].TR;
	}
	else {
		log_msg("ERROR: incorrect device type in fwdMechCableCoupling");
		return;
	}

	// Forward Cable Coupling equations
	//   Originally based on 11/7/2005, Mitch notebook pg. 169
	//   Updated from UCSC code.  Code simplified by HK 8/11
	th1 = (1.0/tr1) * m1;

	// DELETEME
	//	th2 = (1.0/tr2) * m2; // additional CC terms added 3/13
	//	d4  = (1.0/tr4) * m4;

	th2 = (1.0/tr2) * m2 - CABLE_COUPLING_01 * th1;
	d4  = (1.0/tr4) * m4 - CABLE_COUPLING_02 * th1 - CABLE_COUPLING_12 * th2;


	// TODO:: Update with new cable coupling terms
	th1_dot = (1.0/tr1) * m1_dot;
	th2_dot = (1.0/tr2) * m2_dot;
	d4_dot  = (1.0/tr4) * m4_dot;

	// Tool degrees of freedom ===========================================
    int sgn = 0;
    switch(mech->mech_tool.t_style){
	  case raven:
		sgn = (mech->type == GOLD_ARM) ? 1 : -1;
		break;
	  case dv:
		sgn = (mech->type != GOLD_ARM) ? 1 : -1;
		break;
	  case square_raven:
		sgn = -1;
		break;
	  default:
		log_msg("undefined tool style!!! inv_cable_coupling.cpp");
		break;
    }

    float tool_coupling = mech->mech_tool.wrist_coupling;
    th3 = (1.0/tr3) * (m3 - sgn*m4/GB_RATIO);
    th5 = (1.0/tr5) * (m5 - sgn*m4/GB_RATIO);
    th6 = (1.0/tr6) * (m6 - sgn*m4/GB_RATIO) - th5*tool_coupling;
    th7 = (1.0/tr7) * (m7 - sgn*m4/GB_RATIO) + th5*tool_coupling;

#ifndef simulator
	// Now have solved for th1, th2, d3, th4, th5, th6
	mech->joint[SHOULDER].jpos 		= th1;// - mech->joint[SHOULDER].jpos_off;
	mech->joint[ELBOW].jpos 		= th2;// - mech->joint[ELBOW].jpos_off;
	mech->joint[TOOL_ROT].jpos 		= th3;// - mech->joint[TOOL_ROT].jpos_off;
	mech->joint[Z_INS].jpos 		= d4;//  - mech->joint[Z_INS].jpos_off;
	mech->joint[WRIST].jpos 		= th5;// - mech->joint[WRIST].jpos_off;
	mech->joint[GRASP1].jpos 		= th6;// - mech->joint[GRASP1].jpos_off;
	mech->joint[GRASP2].jpos 		= th7;// - mech->joint[GRASP2].jpos_off;

	mech->joint[SHOULDER].jvel 		= th1_dot;// - mech->joint[SHOULDER].jpos_off;
	mech->joint[ELBOW].jvel 		= th2_dot;// - mech->joint[ELBOW].jpos_off;
	mech->joint[Z_INS].jvel 		= d4_dot;//  - mech->joint[Z_INS].jpos_off;
#else
#ifdef dyn_simulator
	// Estimate the current joint positions using dynamic model
        //int current_runlevel = (mech->inputs & (0x40 | 0x80)) >> 6;
	//if (current_runlevel == 3)
        if (abs(mech->joint[SHOULDER].jpos)>0.1)
	{
	 	if (mech->type == GREEN_ARM)
		{	
		      state_type current_state = {(double)mech->joint[SHOULDER].jpos, (double)mech->joint[ELBOW].jpos, 
		                                  (double)mech->joint[Z_INS].jpos, (double) mech->joint[TOOL_ROT].jpos, 
		                                  (double)mech->joint[SHOULDER].jvel,(double)mech->joint[ELBOW].jvel ,(double)mech->joint[Z_INS].jvel,0.0};

		      r_d_green = {(double)mech->joint[SHOULDER].jpos_d, (double)mech->joint[ELBOW].jpos_d, 
		                                  (double)mech->joint[Z_INS].jpos_d, (double) mech->joint[TOOL_ROT].jpos_d, 
		                                  0.0, 0.0,0.0,0.0};
		      clock_gettime(CLOCK_REALTIME,&t1);
		      integrate_adaptive(rk4(), sys_dyn_green, current_state, 0.0, 0.001, 0.0001);  
		      clock_gettime(CLOCK_REALTIME,&t2);
		      double duration=  double((double)t2.tv_nsec/1000 - (double)t1.tv_nsec/1000);

		      log_file("Green Arm Dynamics: %f ms\n",duration);

		      mech->joint[SHOULDER].jpos 	= (float)current_state[0];
		      mech->joint[ELBOW].jpos    	= (float)current_state[1];
	  	      mech->joint[Z_INS].jpos           = (float)current_state[2];        
		      mech->joint[TOOL_ROT].jpos 	= (float)current_state[3];
	
		      mech->joint[SHOULDER].jvel 	= (float)current_state[4];
		      mech->joint[ELBOW].jvel 		= (float)current_state[5];
		      mech->joint[Z_INS].jvel 		= (float)current_state[6];

		      mech->joint[WRIST].jpos 		= mech->joint[WRIST].jpos_d;// - mech->joint[WRIST].jpos_off;
		      mech->joint[GRASP1].jpos 		= mech->joint[GRASP1].jpos_d;// - mech->joint[GRASP1].jpos_off;
		      mech->joint[GRASP2].jpos 		= mech->joint[GRASP2].jpos_d;// - mech->joint[GRASP2].jpos_off;
		}
		else if (mech->type == GOLD_ARM)
		{

		     state_type current_state = {(double)mech->joint[SHOULDER].jpos, (double)mech->joint[ELBOW].jpos, 
		                                  (double)mech->joint[Z_INS].jpos, (double) mech->joint[TOOL_ROT].jpos, 
		                                  (double)mech->joint[SHOULDER].jvel,(double)mech->joint[ELBOW].jvel ,(double)mech->joint[Z_INS].jvel,0.0};

		      r_d_gold = {(double)mech->joint[SHOULDER].jpos_d, (double)mech->joint[ELBOW].jpos_d, 
		                                  (double)mech->joint[Z_INS].jpos_d, (double) mech->joint[TOOL_ROT].jpos_d, 
		                                  0.0, 0.0,0.0,0.0};
		      clock_gettime(CLOCK_REALTIME,&t1);
		      integrate_adaptive(rk4(), sys_dyn_gold, current_state, 0.0, 0.001, 0.0001);  
		      clock_gettime(CLOCK_REALTIME,&t2);
		      double duration=  double((double)t2.tv_nsec/1000 - (double)t1.tv_nsec/1000);

		      log_file("Gold Arm Dynamics: %f ms\n",duration);

		      mech->joint[SHOULDER].jpos 	= (float)current_state[0];
		      mech->joint[ELBOW].jpos    	= (float)current_state[1];
	  	      mech->joint[Z_INS].jpos           = (float)current_state[2];        
		      mech->joint[TOOL_ROT].jpos 	= (float)current_state[3];
	
		      mech->joint[SHOULDER].jvel 	= (float)current_state[4];
		      mech->joint[ELBOW].jvel 		= (float)current_state[5];
		      mech->joint[Z_INS].jvel 		= (float)current_state[6];


		      mech->joint[WRIST].jpos 		= mech->joint[WRIST].jpos_d;// - mech->joint[WRIST].jpos_off;
		      mech->joint[GRASP1].jpos 		= mech->joint[GRASP1].jpos_d;// - mech->joint[GRASP1].jpos_off;
		      mech->joint[GRASP2].jpos 		= mech->joint[GRASP2].jpos_d;// - mech->joint[GRASP2].jpos_off;
		}
	}
	else
	{
		// Short-circuiting: Assumes a perfect model for the last three degrees of freedom
		mech->joint[SHOULDER].jpos 		= mech->joint[SHOULDER].jpos_d;// - mech->joint[SHOULDER].jpos_off;
		mech->joint[ELBOW].jpos 		= mech->joint[ELBOW].jpos_d;// - mech->joint[ELBOW].jpos_off;
		mech->joint[TOOL_ROT].jpos 		= mech->joint[TOOL_ROT].jpos_d;// - mech->joint[TOOL_ROT].jpos_off;
		mech->joint[Z_INS].jpos 		= mech->joint[Z_INS].jpos_d;//  - mech->joint[Z_INS].jpos_off;
		mech->joint[WRIST].jpos 		= mech->joint[WRIST].jpos_d;// - mech->joint[WRIST].jpos_off;
		mech->joint[GRASP1].jpos 		= mech->joint[GRASP1].jpos_d;// - mech->joint[GRASP1].jpos_off;
		mech->joint[GRASP2].jpos 		= mech->joint[GRASP2].jpos_d;// - mech->joint[GRASP2].jpos_off;
		// Estimate the current joint velocity using dynamic model
		mech->joint[SHOULDER].jvel 		= th1_dot;// - mech->joint[SHOULDER].jpos_off;
		mech->joint[ELBOW].jvel 		= th2_dot;// - mech->joint[ELBOW].jpos_off;
		mech->joint[Z_INS].jvel 		= d4_dot;//  - mech->joint[Z_INS].jpos_off;
	}
#else
        // Shortc-circuiting: Assume a perfect model for the last three degrees of freedom
	mech->joint[SHOULDER].jpos 		= mech->joint[SHOULDER].jpos_d;// - mech->joint[SHOULDER].jpos_off;
	mech->joint[ELBOW].jpos 		= mech->joint[ELBOW].jpos_d;// - mech->joint[ELBOW].jpos_off;
	mech->joint[TOOL_ROT].jpos 		= mech->joint[TOOL_ROT].jpos_d;// - mech->joint[TOOL_ROT].jpos_off;
	mech->joint[Z_INS].jpos 		= mech->joint[Z_INS].jpos_d;//  - mech->joint[Z_INS].jpos_off;
	mech->joint[WRIST].jpos 		= mech->joint[WRIST].jpos_d;// - mech->joint[WRIST].jpos_off;
	mech->joint[GRASP1].jpos 		= mech->joint[GRASP1].jpos_d;// - mech->joint[GRASP1].jpos_off;
	mech->joint[GRASP2].jpos 		= mech->joint[GRASP2].jpos_d;// - mech->joint[GRASP2].jpos_off;

	mech->joint[SHOULDER].jvel 		= th1_dot;// - mech->joint[SHOULDER].jpos_off;
	mech->joint[ELBOW].jvel 		= th2_dot;// - mech->joint[ELBOW].jpos_off;
	mech->joint[Z_INS].jvel 		= d4_dot;//  - mech->joint[Z_INS].jpos_off;
	/*log_msg("Cable Coupling joint positions: (%f,%f,%f,%f,%f,%f,%f\n)",
               		 mech->joint[0].jpos*180/M_PI, mech->joint[1].jpos*180/M_PI, mech->joint[2].jpos*180/M_PI,
               		 mech->joint[3].jpos*180/M_PI, mech->joint[4].jpos*180/M_PI,mech->joint[5].jpos*180/M_PI,
			 mech->joint[6].jpos*180/M_PI, mech->joint[7].jpos*180/M_PI);*/
#endif
#endif

	return;
}




/**
* \fn void fwdTorqueCoupling(struct device *device0, int runlevel)
* \brief Calls fwdMechTorqueCoupling for each mechanism in device
* \param device0 - pointer to device struct
* \param runlevel - current runlevel
* \return void
* \todo Remove runlevel from args.
*/

void fwdTorqueCoupling(struct device *device0, int runlevel)
{
	//Run fwd cable coupling for each mechanism.
	// This should be run in all runlevels.
	for (int i = 0; i < NUM_MECH; i++)
		fwdMechTorqueCoupling(&(device0->mech[i]));
}

/**
* \fn void fwdMechTorqueCoupling(struct mechanism *mech)
* \param mech
* \return void
*/

void fwdMechTorqueCoupling(struct mechanism *mech)
{
	float th1=0, th2=0, th3=0, th5=0, th6=0, th7=0;
	float th1_dot, th2_dot;
	float d4;
	float d4_dot;
	float m1, m2, m3, m4, m5, m6, m7;
	float m1_dot, m2_dot, m4_dot;
	float tr1=0, tr2=0, tr3=0, tr4=0, tr5=0, tr6=0, tr7=0;

	m1 = mech->joint[SHOULDER].mpos;
	m2 = mech->joint[ELBOW].mpos;
	m3 = mech->joint[TOOL_ROT].mpos;
	m4 = mech->joint[Z_INS].mpos;
	m5 = mech->joint[WRIST].mpos;
	m6 = mech->joint[GRASP1].mpos;
	m7 = mech->joint[GRASP2].mpos;

	m1_dot = mech->joint[SHOULDER].mvel;
	m2_dot = mech->joint[ELBOW].mvel;
	m4_dot = mech->joint[Z_INS].mvel;

	if (mech->type == GOLD_ARM){
		tr1=DOF_types[SHOULDER_GOLD].TR;
		tr2=DOF_types[ELBOW_GOLD].TR;
		tr3=DOF_types[TOOL_ROT_GOLD].TR;
		tr4=DOF_types[Z_INS_GOLD].TR;
		tr5=DOF_types[WRIST_GOLD].TR;
		tr6=DOF_types[GRASP1_GOLD].TR;
		tr7=DOF_types[GRASP2_GOLD].TR;

	} else if (mech->type == GREEN_ARM){
		tr1=DOF_types[SHOULDER_GREEN].TR;
		tr2=DOF_types[ELBOW_GREEN].TR;
		tr3=DOF_types[TOOL_ROT_GREEN].TR;
		tr4=DOF_types[Z_INS_GREEN].TR;
		tr5=DOF_types[WRIST_GREEN].TR;
		tr6=DOF_types[GRASP1_GREEN].TR;
		tr7=DOF_types[GRASP2_GREEN].TR;
	}
	else {
		log_msg("ERROR: incorrect device type in fwdMechCableCoupling");
		return;
	}

	// Forward Cable Coupling equations
	//   Originally based on 11/7/2005, Mitch notebook pg. 169
	//   Updated from UCSC code.  Code simplified by HK 8/11
	th1 = (1.0/tr1) * m1;

	// DELETEME
	//	th2 = (1.0/tr2) * m2; // additional CC terms added 3/13
	//	d4  = (1.0/tr4) * m4;

	th2 = (1.0/tr2) * m2 - CABLE_COUPLING_01 * th1;
	d4  = (1.0/tr4) * m4 - CABLE_COUPLING_02 * th1 -CABLE_COUPLING_12 * th2;


	// TODO:: Update with new cable coupling terms
	th1_dot = (1.0/tr1) * m1_dot;
	th2_dot = (1.0/tr2) * m2_dot;
	d4_dot  = (1.0/tr4) * m4_dot;


	// Tool degrees of freedom ===========================================
	if (mech->tool_type == TOOL_GRASPER_10MM)
	{
		int sgn = (mech->type == GOLD_ARM) ? 1 : -1;
		th3 = (1.0/tr3) * (m3 - sgn*m4/GB_RATIO);
		th5 = (1.0/tr5) * (m5 - sgn*m4/GB_RATIO);
		th6 = (1.0/tr6) * (m6 - sgn*m4/GB_RATIO);
		th7 = (1.0/tr7) * (m7 - sgn*m4/GB_RATIO);
	}

	// Now have solved for th1, th2, d3, th4, th5, th6
	mech->joint[SHOULDER].jpos 	= th1;// - mech->joint[SHOULDER].jpos_off;
	mech->joint[ELBOW].jpos 		= th2;// - mech->joint[ELBOW].jpos_off;
	mech->joint[TOOL_ROT].jpos 	= th3;// - mech->joint[TOOL_ROT].jpos_off;
	mech->joint[Z_INS].jpos 		= d4;//  - mech->joint[Z_INS].jpos_off;
	mech->joint[WRIST].jpos 		= th5;// - mech->joint[WRIST].jpos_off;
	mech->joint[GRASP1].jpos 		= th6;// - mech->joint[GRASP1].jpos_off;
	mech->joint[GRASP2].jpos 		= th7;// - mech->joint[GRASP2].jpos_off;

	mech->joint[SHOULDER].jvel 	= th1_dot;// - mech->joint[SHOULDER].jpos_off;
	mech->joint[ELBOW].jvel 		= th2_dot;// - mech->joint[ELBOW].jpos_off;
	mech->joint[Z_INS].jvel 		= d4_dot;//  - mech->joint[Z_INS].jpos_off;

	return;
}


