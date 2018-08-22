// DataGenerator.cpp : Defines the entry point for the console application.
// Utilization cases: 70% --> 100%, interval = 5;

#ifdef WINDOWS

#include "stdafx.h"
#include "string"
#include <conio.h>

#endif

#include <iostream>
#include <random>
#include <iomanip>
#include <fstream>

using namespace std;

#define CAP              4        // CAPacity of the system (the number of processor)
#define MAX_AP_SET       30
#define MAX_AP_TSK       4
#define INIT_AP_ID       100
#define MAX_OBSERVATION  100000
#define MAX_TSK          100      // maximum number of tasks
#define MAX_ITL          7        // maximum of intervals
#define MAX_SIZE         10       // number of task set/case
#define UTIL_LOWER_BOUND 0.1
#define UTIL_UPPER_BOUND 0.7


int main()
{

	string filename;
	double lb_util = UTIL_LOWER_BOUND;
	double ub_util = UTIL_UPPER_BOUND;
	double util[MAX_ITL][MAX_SIZE][MAX_TSK]; // recode of utilization (70%, 75%, 80%, 85%, 90%, 95% and 100%)
	double num;
	double sum[MAX_ITL][MAX_SIZE], temp_sum;
	default_random_engine generator; // a random generator
	//Generate the Utilization
	for (int k = 0; k < MAX_ITL; k++)// k corresponds to each utilization interval: k=0 -> 70%, k=1 -> 75%, .. and so on.
	{
		for (int exp = 1; exp <= MAX_SIZE; exp += 1) 
		{
			exponential_distribution<double> utildis((double)exp);
			double p[MAX_TSK] = {}; // temperary utilization
			int i = 0;
			sum[k][exp - 1] = 0;
			temp_sum = 0;
			while (i < MAX_TSK) // i correspods to each task
			{
				num = ceil(utildis(generator) * 10) / 10.0;// For convenience of multiprocessor scheduling, the utilizations are expected to be rounded, example: 0.1, 0.2, ... and so on.
				if (num >= lb_util && num <= ub_util) // here we just limit the acceptable utilization among lb_util to ub_util in order to increase the number of tasks
				{                             // If we accept a larger utilization, then the number of tasks is just a few.
					temp_sum = sum[k][exp - 1] + num;      // temporary sum of utilization
					if (temp_sum > CAP * (0.7 + 0.05*k)) // if sum of utilization exceeds the expected total utilization (for example: 70% of processor CAPacity)
					{                                       // then the total utilization is set to the exepected one.
						p[i] = ceil((CAP * (0.7 + 0.05*k) - sum[k][exp - 1])*10)/10.0;
						sum[k][exp - 1] = CAP * (0.7 + 0.05*k);
						break;
					}
					p[i] = num;
					sum[k][exp - 1] = temp_sum;
					if (sum[k][exp - 1] == CAP * (0.7 + 0.05*k)) // if the total utilization is equal to the expected one, then we stop generating tasks
						break;
					i++;
				}
			}
			// copy the temporary utilizations to the record of utilization
			for (int l = 0; l < MAX_TSK; l++)
			{
				util[k][exp - 1][l] = p[l];
			}
		}
	}
	
		//Generate WCET
	uniform_int_distribution<int> wcetdis(1, 20); // Wcet is generated using an uniform distribution
	int wcet[MAX_ITL][MAX_SIZE][MAX_TSK];                           // We here limit the wcet to be integer numbers lower than 20
	int period[MAX_ITL][MAX_SIZE][MAX_TSK];
	double pd = 0;
	int mod;
	for (int i = 0; i < MAX_ITL; i++)
	{
		cout << "Case "<< 70 + 5*i << "%: generating" << endl;
		for (int k = 0; k < MAX_SIZE; k++)
		{
			cout << " Set " << k << " generating" << endl;
			for (int j = 0; j < MAX_TSK; j++)
			{
				if (util[i][k][j]*10 > 0)
				{
					wcet[i][k][j] = wcetdis(generator);
					pd = wcet[i][k][j] / util[i][k][j];
					mod = (int)(pd * 10);
					while (mod % 10)
					{
						cout << "Task "<< j <<": mod= "<< mod <<" wcet=  " << wcet[i][k][j]<<" util=" << util[i][k][j] <<" period=  "<< pd << endl;
						//getch();
						wcet[i][k][j] = wcetdis(generator);
						pd = wcet[i][k][j] / util[i][k][j];
						mod = (int)(pd * 10);
					}
					//cout << "Done: wcet=  " << wcet[i][k][j] << " util=" << util[i][k][j] << " period=  " << pd << endl;
					//getch();
					period[i][k][j] = pd;
				}
					
				else
					wcet[i][k][j] = 0;
			}
			cout << " Set " << k << ": done" << endl;
		}
		cout << "Case " << 70 + 5*i << ": done" << endl;
	}
	
	//	//// Similarly to WCET, we can generate ET and period
	//////Generate ET
	/*int et[MAX_ITL][MAX_SIZE][MAX_TSK];
	for (int i = 0; i < MAX_ITL; i++)
	{
		for (int k = 0; k < MAX_SIZE; k++)
		{
			for (int j = 0; j < MAX_TSK; j++)
			{
				if (util[i][k][j] > 0)
				{
					uniform_int_distribution<int> etdis(1, wcet[i][k][j]);
					et[i][k][j] = (int)etdis(generator);
				}
				else
					et[i][k][j] = 0;
			}
		}
	}*/
	//////Generate Period
	//int period[MAX_ITL][MAX_SIZE][MAX_TSK];
	//for (int i = 0; i < MAX_ITL; i++)
	//{
	//	for (int k = 0; k < MAX_SIZE; k++)
	//	{
	//		for (int j = 0; j < MAX_TSK; j++)
	//		{
	//			if (util[i][k][j] > 0)
	//				//period[i][k][j] = (int)ceil(((wcet[i][k][j] * 10) / (util[i][k][j] * 10)));
	//				period[i][k][j] = (wcet[i][k][j] * 10) / (util[i][k][j] * 10);
	//			else
	//				period[i][k][j] = 0;
	//		}
	//	}
	//}
	// print the generated utilizations to the terminal screen
	cout << "Generated data:  " << endl;
	for (int i = 0; i < MAX_ITL; i++)
	{
		cout << 70 + i * 5 << "%:" << endl;
		for (int k = 0; k < MAX_SIZE; k++)
		{
			cout << "Set " << k << ": " << sum[i][k] << ": ";
			for (int j = 0; util[i][k][j] > 0; j++)
			{
				cout << util[i][k][j] << " ";
			}
			cout << endl;
			cout << "      wcet: ";
			for (int j = 0; wcet[i][k][j] > 0; j++)
			{
				cout << wcet[i][k][j] << " ";
			}
			cout << endl;
			cout << "    period: ";
			for (int j = 0; period[i][k][j] > 0; j++)
			{
				cout << period[i][k][j] << " ";
			}
			//cout << endl;
			cout << endl;
		}
		cout << endl;
	}
	// Create output data
	int et;
	for (int i = 0; i < MAX_ITL; i++)
	{
		for (int k = 0; k < MAX_SIZE; k++)
		{
			ofstream outputfile;
			filename = "periodic.cfg." + to_string(CAP) + "-" + to_string(70 + 5 * i) + "-" + to_string(k+1);
			outputfile.open(filename.c_str());
			outputfile << "#Target Periodic Utilization = " << 70 + 5 * i << "%" << endl;
			outputfile << "#tid\twcet\tet\tprd\treq_tim" << endl;
			for (int j = 0; j < MAX_TSK; j++)
			{
				if (util[i][k][j] > 0)
				{
					for (int l = 0; l*period[i][k][j] < MAX_OBSERVATION; l++)
					{
						uniform_int_distribution<int> etdis(1, wcet[i][k][j]);
						et = (int)etdis(generator);
						outputfile << j << "\t" << wcet[i][k][j] << "\t" << et << "\t" << period[i][k][j] << "\t" << l*period[i][k][j] << endl;
					}
				}
			}
			outputfile.close();
		}
	}
	// //Generating the aperiodic tasks
	//int ap_wcet[max_ap_set][MAX_AP_TSK];
	//int count;
	//for (int i = 0; i < max_ap_set; i++)
	//{
	//	for (int j = 0; j < MAX_AP_TSK; j++)
	//	{
	//		uniform_int_distribution<int> wcetdis(1, 20); // Wcet is generated using an uniform distribution
	//		ap_wcet[i][j] = wcetdis(generator);
	//	}
	//}

	////// Generating aperiodic wcet
	//cout << "Aperiodic WCET:  " << endl;
	//for (int i = 0; i < max_ap_set; i++)
	//{
	//	cout << "Set " << i+1 << ": ";
	//	for (int j = 0; j < MAX_AP_TSK; j++)
	//	{
	//		cout << ap_wcet[i][j] << " ";
	//	}
	//	cout << endl;
	//}
	//cout << endl;

	////////Generating aperiodic data
	//int ap_et;
	//int ap_req;
	//int ap_rel_dis;
	//int dis_sum;
	//int rel_num;
	//int ap_rel;
	//double ap_util[MAX_AP_TSK];
	//double ap_util_sum;
	//double ap_dis_average;
	//int et_sum;
	//double et_average;
	//streampos text_line_positions;
	//text_line_positions = 1;
	//for (int i = 0; i < max_ap_set; i++)
	//{
	//	ap_util_sum = 0;
	//	ap_rel = 0;
	//	ofstream outputfile;
	//	filename = "aperiodic.cfg." + to_string(CAP) + "-" + to_string(i+1);
	//	outputfile.open(filename.c_str());
	//	text_line_positions = outputfile.tellp();
	//	outputfile << "\#Target Aperiodic Tasks" << endl;
	//	outputfile << "#Release No.: " << 100000 << ", Total Util:" << 0.111111111 << endl;
	//	outputfile << "\#tid\twcet\tet\tprd\treq_tim" << endl;
	//	for (int j = 0; j < MAX_AP_TSK; j++)
	//	{
	//		ap_req = 0;
	//		dis_sum = 0;
	//		rel_num = 0;
	//		ap_util[j] = 0;
	//		ap_dis_average = 0;
	//		et_average = 0;
	//		et_sum = 0;
	//		while (ap_req < MAX_OBSERVATION)
	//		{
	//			uniform_int_distribution<int> etdis(1, ap_wcet[i][j]);
	//			uniform_int_distribution<int> reqdis(20, 100);
	//			ap_et = etdis(generator);
	//			ap_rel_dis = reqdis(generator);
	//			ap_req = ap_req + ap_rel_dis;
	//			dis_sum += ap_rel_dis;
	//			rel_num++;
	//			et_sum += ap_et;
	//			outputfile << INIT_AP_ID + j << "\t" << ap_wcet[i][j] << "\t" << ap_et << "\t" << 0 << "\t" << ap_req << endl;
	//		}
	//		ap_dis_average = dis_sum / rel_num;
	//		ap_util[j] = ap_wcet[i][j] / ap_dis_average;
	//		et_average = et_sum / rel_num;
	//		ap_util_sum += ap_util[j];
	//		ap_rel += rel_num;
	//	}
	//	//cout << "Generated file: " << filename.c_str() << endl;
	//	cout << "Release No.: " << ap_rel << " Total Util:" << ap_util_sum << endl;
	//	outputfile.seekp(text_line_positions);
	//	outputfile << "\#Target Aperiodic Tasks" << endl;
	//	outputfile << "#Release No.: " << ap_rel << ", Total Util:" << ap_util_sum ;
	//	outputfile.close();
	//	
	//}

	cout << "end!" << endl;
	system("pause");
	return 0;
}
