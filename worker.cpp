#include <signal.h> //sigaction
#include <iostream>
#include <cstring>
#include <errno.h> //perror
#include <sys/types.h> //open
#include <sys/stat.h> //open
#include <fcntl.h> //open k flags
#include <unistd.h> //read, write
#include <fstream>
#include "worker.h"
#include "utils.h"
#include "record.h"
#include "record_HT.h"
#include "cdHashTable.h"

int summary_entries =0;

//pernaei telika tis eggrafes kai stous allous HT afou exoun ginei oi elegxoi
void populate_other_HTs(record_HT * rht , diseaseHashTable * dht, countryHashTable * cht){
  for(unsigned int i=0; i<rht->size; i++){
    if(rht->table[i] == NULL) //kenh alusida
      continue;
    else{
      record_HT_node * currptr = rht->table[i];
      while(currptr!= NULL){ //paei mexri kai to teleutaio. insert ta records poy exei ekei mesa o rht
        //std::cout << currptr->rec_ptr->get_recordID() << " " << currptr->rec_ptr->get_diseaseID() << " " << currptr->rec_ptr->get_patientFirstName() << " " << currptr->rec_ptr->get_entryDate() << " " <<currptr->rec_ptr->get_exitDate() <<"\n";
        dht->insert_record(currptr->rec_ptr);
        cht->insert_record(currptr->rec_ptr);
        currptr = currptr->next ;
      }//telos while gia orizontia lista
    }//telos else
  }//telos for gia kathe alusida

}

//diabazei arxeio kai kanei populate tis domes (apo 1h ergasia oi perissoteres)
void parse_records_from_file(std::string filename, std::string date, std::string folder, record_HT * rht, file_summary* summ){
  std::ifstream infile(filename.c_str()); //diabasma apo tis grammes tou arxeiou
  std::string line; //EPITREPETAI H STRING EIPAN STO PIAZZA
  if(is_date_ok(date) == false) //an to date onoma arxeiou den einai hmeromhnia, asto
    return;

    //std::cout << " my file is " << filename << "\n";
  while (std::getline(infile, line)){
    //https://stackoverflow.com/questions/49201654/splitting-a-string-with-multiple-delimiters-in-c
    std::string const delims{ " \t\r\n" }; //delimiters einai ta: space,tab kai carriage return. TELOS.
    size_t beg, pos = 0;
    int params_count =0;
    std::string true_record_parts[8];
    true_record_parts[5] = "-"; //gia entry d
    true_record_parts[6] = "-"; //gia exit d
    std::string record_parts[6]; //mia thesi gia kathe melos tou record opws prpei na einai sto arxeio
    while ((beg = line.find_first_not_of(delims, pos)) != std::string::npos){
        pos = line.find_first_of(delims, beg + 1);
        record_parts[params_count] = line.substr(beg, pos - beg);
        params_count++;
    }//telos while eksagwghs gnwrismatwn apo grammh
    if(params_count != 6) //kati leipei/pleonazei, akurh h eggrafh!
      {std::cout<< "ERROR\n";continue;}
    //fernw thn eggrafh sth morfh ths ergasias 1 gia na einai apodotika kai eukolotera ta queries
    true_record_parts[0] = record_parts[0]; //id
    true_record_parts[1] = record_parts[2]; //first name
    true_record_parts[2] = record_parts[3]; //last name
    true_record_parts[3] = record_parts[4]; //disease
    true_record_parts[4] = folder; //country
    if(record_parts[1] == "ENTER")
      true_record_parts[5] = date; //entrydate to onoma tou arxeiou
    else if(record_parts[1] == "EXIT")
      true_record_parts[6] = date; //exitdate to onoma tou arxeiou
    else //kakh eggrafh, aporripsh k sunexeia
      {std::cout<< "ERROR\n";continue;}
    true_record_parts[7] = record_parts[5]; //age
    if(stoi(true_record_parts[7]) < 0)//arnhtiko age, proxwrame
      {std::cout<< "ERROR\n";continue;}
    record * new_rec_ptr = new record(true_record_parts); //dhmiourgia eggrafhs
    //std::cout << new_rec_ptr->get_recordID() << " " << new_rec_ptr->get_patientFirstName() << " " << new_rec_ptr->get_age() << " " << new_rec_ptr->get_entryDate()<< " " << new_rec_ptr->get_country() << "\n";
    //TO PERNAW STIS DOMES ME ELEGXO GIA EXIT AN YPARXEI KTL!!
    int parsed = rht->insert_record(new_rec_ptr);
    if(parsed < 0)
      {std::cout<< "ERROR\n";continue;} //den egine insert gt exei problhma, pame epomenh
    //PAME na perasoume thn plhroforia poy phrame sth domh summary
    if(summ->insert_data(true_record_parts) == 1)
      summary_entries += 1; //mphke kainourgia astheneia

  }//telos while diabasmatos arxeiou, pername tis eggrafes stous alloues 2HT ths askhshs 1
  //std::cout << "i was " << filename << " " << summ->diseasename <<" "<< summ->age_cats[0] << "\n";
}

int work(char * read_pipe, char * write_pipe, int bsize){

  int read_fd, write_fd;
  char sbuf[500];
  char jbuf[500];
  int n_dirs=0;
  int n_files=0;
  //oi domes moy. Enas aplos HT gia eggrafes kai oi HTs apo thn ergasia 1
  record_HT records_htable(50); //o DIKOS MOU HT gia tis eggrafes basei recordID megethous h1+h2. KALUTEROS APO APLH LISTA
  diseaseHashTable diseases_htable(25, 64); //O erg1 HT GIA DISEASE
  countryHashTable countries_htable(25, 64); //O erg1 HT GIA COUNTRY



  //DIABAZW DIRECTORIES POY MOY EDWSE O GONIOS
    read_fd = open(read_pipe, O_RDONLY );
    read(read_fd, &n_dirs, sizeof(int));
    std::string * countries = new std::string[n_dirs]; //onomata xwrwn poy exw
    std::string * date_files = NULL; //tha mpoun ta filesnames-hmeromhnies
    for(int i=0; i<n_dirs; i++){
      n_files=0;
      receive_string(read_fd, &(countries[i]), bsize ); //pairnw prwta xwra
      receive_string(read_fd, sbuf, bsize ); //pairnw olo to path
      extract_files(sbuf, &n_files, &date_files); //pairnw plhrofories
      sort_files(date_files,0 ,n_files-1); //sort by date gia pio swsto parsing
      for(int j=0; j<n_files; j++){
        summary_entries=0; //gia na kserw ti tha pw sto gonio
        file_summary * mysum = new file_summary; //boh8htikh domh gia to summary poy tha stelnei meta apo kathe arxeio sto gonio
        strcpy(jbuf, "");
        sprintf(jbuf, "%s/%s",sbuf, (date_files[j]).c_str());
        parse_records_from_file(std::string(jbuf), date_files[j] ,countries[i], &records_htable, mysum);
        delete mysum;
      }

      //std::cout << getpid() << " diabasa dir ap par " << sbuf << "\n";
      delete[] date_files; //sbhse to new poy egine
    }
    //close(read_fd); //to afhnw anoixto
    delete[] countries; //svhse to new poy egine
    populate_other_HTs(&records_htable, &diseases_htable, &countries_htable); //perna tis eggrafes sou kai stous allous 2 pinakes askhshs 1
    //records_htable.print_contents();
    //diseases_htable.print_contents();
    //countries_htable.print_contents();

    //STELNW STO GONIO TA SUMMARY STATISTICS

    //enhmerwnw gonio oti teleiwsa to parsing
    write_fd = open(write_pipe, O_WRONLY );
    //write(write_fd, "meow", strlen("mewo") +1);
    send_string(write_fd, "ok", bsize);
    //close(write_fd); // to afhnw anoixto

  //sleep(4);

  strcpy(sbuf, "");
  char sbuf2[200];
  strcpy(sbuf2, "");
  std::string tool;

  //arxizw na pairnw entols xrhsth
  while(1){


        int rdb = receive_string(read_fd, &tool, bsize);
        while(tool == ""){
          //rdb = receive_string(read_fd, sbuf2, bsize);
          rdb = receive_string(read_fd, &tool, bsize);
        }
        //std::cout << "diabas apo gonio "<< tool << getpid() <<"\n";


        if(tool == "/exit"){
          //isws cleanup??
          break;
        }
        else if(tool == "bad"){
          ;;//send_string(write_fd, "meow", bsize);
        }
        else if(tool == "/diseaseFrequency1"){ //xwris orisma country
          std::string dis_name;
          rdb = receive_string(read_fd, &dis_name, bsize); //diabase astheneia
          std::string date1;
          rdb = receive_string(read_fd, &date1, bsize); //diabase date1
          std::string date2;
          rdb = receive_string(read_fd, &date2, bsize); //diabase date2
          int number_to_present = diseases_htable.total_recs_for_cat(dis_name, date1, date2);
          //std::cout << dis_name << " ^ " << number_to_present << "\n";
          write(write_fd, &number_to_present, sizeof(int)); //tou stelnw to zhtoumeno noumero
        }
        else if(tool == "/diseaseFrequency2"){ //ME orisma country
          std::string dis_name;
          rdb = receive_string(read_fd, &dis_name, bsize); //diabase astheneia
          std::string date1;
          rdb = receive_string(read_fd, &date1, bsize); //diabase date1
          std::string date2;
          rdb = receive_string(read_fd, &date2, bsize); //diabase date2
          std::string country;
          rdb = receive_string(read_fd, &country, bsize); //diabase date2
          int number_to_present = diseases_htable.total_recs_for_cat(dis_name, date1, date2, country);
          //std::cout << dis_name << " ^ " << number_to_present << "\n";
          write(write_fd, &number_to_present, sizeof(int)); //tou stelnw to zhtoumeno noumero
        }
        else{
          std::cout << "diabas apo gonio "<< tool << getpid() <<"\n";
        }


  }



  close(read_fd);
  close(write_fd);
  //std::cout << "eftasa\n";
  return 0;


}
