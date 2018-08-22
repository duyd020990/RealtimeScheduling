// DataGenerator.cpp : Defines the entry point for the console application.
// Utilization cases: 70% --> 100%, interval = 5;

#include "stdafx.h"
#include <iostream>
#include "string"
#include <random>
#include <conio.h>
#include <iomanip>
#include <fstream>

int main()
{
	const int cap = 4; // capacity of the system (the number of processor)
	const int max_ap_set = 30;
	const int max_ap_tsk = 4;
	const int init_ap_id = 100;
	const int max_observation = 100000;
	const int max_tsk = 100; // maximum number of tasks
	const int max_itl = 7; // maximum of intervals
	const int max_size = 10; // number of task set/case
	std::string filename;
	double lb_util = 0.1;
	double ub_util = 0.7;
	double util[max_itl][max_size][max_tsk]; // recode of utilization (70%, 75%, 80%, 85%, 90%, 95% and 100%)
	double num;
	double sum[max_itl][max_size], temp_sum;
	std::default_random_engine generator; // a random generator
	//Generate the Utilization
	for (int k = 0; k < max_itl; k++)// k corresponds to each utilization interval: k=0 -> 70%, k=1 -> 75%, .. and so on.
	{
		for (int exp = 1; exp <= max_size; exp += 1) 
		{
			std::exponential_distribution<double> utildis((double)exp);
			double p[max_tsk] = {}; // temperary utilization
			int i = 0;
			sum[k][exp - 1] = 0;
			temp_sum = 0;
			while (i < max_tsk) // i correspods to each task
			{
				num = ceil(utildis(generator) * 10) / 10.0;// For convenience of multiprocessor scheduling, the utilizations are expected to be rounded, example: 0.1, 0.2, ... and so on.
				if (num >= lb_util && num <= ub_util) // here we just limit the acceptable utilization among lb_util to ub_util in order to increase the number of tasks
				{                             // If we accept a larger utilization, then the number of tasks is just a few.
					temp_sum = sum[k][exp - 1] + num;      // temporary sum of utilization
					if (temp_sum > cap * (0.7 + 0.05*k)) // if sum of utilization exceeds the expected total utilization (for example: 70% of processor capacity)
					{                                       // then the total utilization is set to the exepected one.
						p[i] = ceil((cap * (0.7 + 0.05*k) - sum[k][exp - 1])*10)/10.0;
						sum[k][exp - 1] = cap * (0.7 + 0.05*k);
						break;
					}
					p[i] = num;
					sum[k][exp - 1] = temp_sum;
					if (sum[k][exp - 1] == cap * (0.7 + 0.05*k)) // if the total utilization is equal to the expected one, then we stop generating tasks
						break;
					i++;
				}
			}
			// copy the temporary utilizations to the record of utilization
			for (int l = 0; l < max_tsk; l++)
			{
				util[k][exp - 1][l] = p[l];
			}
		}
	}
	
		//Generate WCET
	std::uniform_int_distribution<int> wcetdis(1, 20); // Wcet is generated using an uniform distribution
	int wcet[max_itl][max_size][max_tsk];                           // We here limit the wcet to be integer numbers lower than 20
	int period[max_itl][max_size][max_tsk];
	double pd = 0;
	int mod;
	for (int i = 0; i < max_itl; i++)
	{
		std::cout << "Case "<< 70 + 5*i << "%: generating" << std::endl;
		for (int k = 0; k < max_size; k++)
		{
			std::cout << " Set " << k << " generating" << std::endl;
			for (int j = 0; j < max_tsk; j++)
			{
				if (util[i][k][j]*10 > 0)
				{
					wcet[i][k][j] = wcetdis(generator);
					pd = wcet[i][k][j] / util[i][k][j];
					mod = (int)(pd * 10);
					while (mod % 10)
					{
						std::cout << "Task "<< j <<": mod= "<< mod <<" wcet=  " << wcet[i][k][j]<<" util=" << util[i][k][j] <<" period=  "<< pd << std::endl;
						//getch();
						wcet[i][k][j] = wcetdis(generator);
						pd = wcet[i][k][j] / util[i][k][j];
						mod = (int)(pd * 10);
					}
					//std::cout << "Done: wcet=  " << wcet[i][k][j] << " util=" << util[i][k][j] << " period=  " << pd << std::endl;
					//getch();
					period[i][k][j] = pd;
				}
					
				else
					wcet[i][k][j] = 0;
			}
			std::cout << " Set " << k << ": done" << std::endl;
		}
		std::cout << "Case " << 70 + 5*i << ": done" << std::endl;
	}
	
	//	//// Similarly to WCET, we can generate ET and period
	//////Generate ET
	/*int et[max_itl][max_size][max_tsk];
	for (int i = 0; i < max_itl; i++)
	{
		for (int k = 0; k < max_size; k++)
		{
			for (int j = 0; j < max_tsk; j++)
			{
				if (util[i][k][j] > 0)
				{
					std::uniform_int_distribution<int> etdis(1, wcet[i][k][j]);
					et[i][k][j] = (int)etdis(generator);
				}
				else
					et[i][k][j] = 0;
			}
		}
	}*/
	//////Generate Period
	//int period[max_itl][max_size][max_tsk];
	//for (int i = 0; i < max_itl; i++)
	//{
	//	for (int k = 0; k < max_size; k++)
	//	{
	//		for (int j = 0; j < max_tsk; j++)
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
	std::cout << "Generated data:  " << std::endl;
	for (int i = 0; i < max_itl; i++)
	{
		std::cout << 70 + i * 5 << "%:" << std::endl;
		for (int k = 0; k < max_size; k++)
		{
			std::cout << "Set " << k << ": " << sum[i][k] << ": ";
			for (int j = 0; util[i][k][j] > 0; j++)
			{
				std::cout << util[i][k][j] << " ";
			}
			std::cout << std::endl;
			std::cout << "      wcet: ";
			for (int j = 0; wcet[i][k][j] > 0; j++)
			{
				std::cout << wcet[i][k][j] << " ";
			}
			std::cout << std::endl;
			std::cout << "    period: ";
			for (int j = 0; period[i][k][j] > 0; j++)
			{
				std::cout << period[i][k][j] << " ";
			}
			//std::cout << std::endl;
			std::cout << std::endl;
		}
		std::cout << std::endl;
	}
	// Create output data
	int et;
	for (int i = 0; i < max_itl; i++)
	{
		for (int k = 0; k < max_size; k++)
		{
			std::ofstream outputfile;
			filename = "periodic.cfg." + std::to_string(cap) + "-" + std::to_string(70 + 5 * i) + "-" + std::to_string(k+1);
			outputfile.open(filename.c_str());
			outputfile << "\#Target Periodic Utilization = " << 70 + 5 * i << "%" << std::endl;
			outputfile << "\#tid\twcet\tet\tprd\treq_tim" << std::endl;
			for (int j = 0; j < max_tsk; j++)
			{
				if (util[i][k][j] > 0)
				{
					for (int l = 0; l*period[i][k][j] < max_observation; l++)
					{
						std::uniform_int_distribution<int> etdis(1, wcet[i][k][j]);
						et = (int)etdis(generator);
						outputfile << j << "\t" << wcet[i][k][j] << "\t" << et << "\t" << period[i][k][j] << "\t" << l*period[i][k][j] << std::endl;
					}
				}
			}
			outputfile.close();
		}
	}
	// //Generating the aperiodic tasks
	//int ap_wcet[max_ap_set][max_ap_tsk];
	//int count;
	//for (int i = 0; i < max_ap_set; i++)
	//{
	//	for (int j = 0; j < max_ap_tsk; j++)
	//	{
	//		std::uniform_int_distribution<int> wcetdis(1, 20); // Wcet is generated using an uniform distribution
	//		ap_wcet[i][j] = wcetdis(generator);
	//	}
	//}

	////// Generating aperiodic wcet
	//std::cout << "Aperiodic WCET:  " << std::endl;
	//for (int i = 0; i < max_ap_set; i++)
	//{
	//	std::cout << "Set " << i+1 << ": ";
	//	for (int j = 0; j < max_ap_tsk; j++)
	//	{
	//		std::cout << ap_wcet[i][j] << " ";
	//	}
	//	std::cout << std::endl;
	//}
	//std::cout << std::endl;

	////////Generating aperiodic data
	//int ap_et;
	//int ap_req;
	//int ap_rel_dis;
	//int dis_sum;
	//int rel_num;
	//int ap_rel;
	//double ap_util[max_ap_tsk];
	//double ap_util_sum;
	//double ap_dis_average;
	//int et_sum;
	//double et_average;
	//std::streampos text_line_positions;
	//text_line_positions = 1;
	//for (int i = 0; i < max_ap_set; i++)
	//{
	//	ap_util_sum = 0;
	//	ap_rel = 0;
	//	std::ofstream outputfile;
	//	filename = "aperiodic.cfg." + std::to_string(cap) + "-" + std::to_string(i+1);
	//	outputfile.open(filename.c_str());
	//	text_line_positions = outputfile.tellp();
	//	outputfile << "\#Target Aperiodic Tasks" << std::endl;
	//	outputfile << "#Release No.: " << 100000 << ", Total Util:" << 0.111111111 << std::endl;
	//	outputfile << "\#tid\twcet\tet\tprd\treq_tim" << std::endl;
	//	for (int j = 0; j < max_ap_tsk; j++)
	//	{
	//		ap_req = 0;
	//		dis_sum = 0;
	//		rel_num = 0;
	//		ap_util[j] = 0;
	//		ap_dis_average = 0;
	//		et_average = 0;
	//		et_sum = 0;
	//		while (ap_req < max_observation)
	//		{
	//			std::uniform_int_distribution<int> etdis(1, ap_wcet[i][j]);
	//			std::uniform_int_distribution<int> reqdis(20, 100);
	//			ap_et = etdis(generator);
	//			ap_rel_dis = reqdis(generator);
	//			ap_req = ap_req + ap_rel_dis;
	//			dis_sum += ap_rel_dis;
	//			rel_num++;
	//			et_sum += ap_et;
	//			outputfile << init_ap_id + j << "\t" << ap_wcet[i][j] << "\t" << ap_et << "\t" << 0 << "\t" << ap_req << std::endl;
	//		}
	//		ap_dis_average = dis_sum / rel_num;
	//		ap_util[j] = ap_wcet[i][j] / ap_dis_average;
	//		et_average = et_sum / rel_num;
	//		ap_util_sum += ap_util[j];
	//		ap_rel += rel_num;
	//	}
	//	//std::cout << "Generated file: " << filename.c_str() << std::endl;
	//	std::cout << "Release No.: " << ap_rel << " Total Util:" << ap_util_sum << std::endl;
	//	outputfile.seekp(text_line_positions);
	//	outputfile << "\#Target Aperiodic Tasks" << std::endl;
	//	outputfile << "#Release No.: " << ap_rel << ", Total Util:" << ap_util_sum ;
	//	outputfile.close();
	//	
	//}

	std::cout << "end!" << std::endl;
	getch();
	return 0;
}
