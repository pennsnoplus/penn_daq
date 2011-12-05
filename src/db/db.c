#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "packet_types.h"
#include "db_types.h"

#include "main.h"
#include "pouch.h"
#include "json.h"
#include "db.h"

int get_new_id(char* newid)
{
  char get_db_address[500];
  sprintf(get_db_address,"%s/_uuids",DB_SERVER);
  pouch_request *pr = pr_init();
  pr_set_url(pr, get_db_address);
  pr_set_method(pr, GET);
  pr_do(pr);
  JsonNode *newerid = json_decode(pr->resp.data);
  int ret = sprintf(newid,"%s",json_get_string(json_find_element(json_find_member(newerid,"uuids"),0)));
  json_delete(newerid);
  pr_free(pr);
  if (ret)
    return 0;
  return 1;
}

int parse_mtc(JsonNode* value,mtc_t* mtc)
{
    int i;
    JsonNode* mtcd = json_find_member(value,"mtcd");
    JsonNode* mtca  = json_find_member(value,"mtca");
    JsonNode* nhit = json_find_member(mtca,"nhit");
    JsonNode* esum = json_find_member(mtca,"esum");
    JsonNode* spare = json_find_member(mtca,"spare");

    mtc->mtcd.lockout_width = (int)json_get_number(json_find_member(mtcd,"lockout_width"));
    mtc->mtcd.pedestal_width = (int)json_get_number(json_find_member(mtcd,"pedestal_width"));
    mtc->mtcd.nhit100_lo_prescale = (int)json_get_number(json_find_member(mtcd,"nhit100_lo_prescale"));
    mtc->mtcd.pulser_period = (int)json_get_number(json_find_member(mtcd,"pulser_period"));
    mtc->mtcd.low10Mhz_clock = (int)json_get_number(json_find_member(mtcd,"low10Mhz_clock"));
    mtc->mtcd.high10Mhz_clock = (int)json_get_number(json_find_member(mtcd,"high10Mhz_clock"));
    mtc->mtcd.fine_slope = (float) json_get_number(json_find_member(mtcd,"fine_slope"));
    mtc->mtcd.min_delay_offset = (float) json_get_number(json_find_member(mtcd,"min_delay_offset"));
    mtc->mtcd.coarse_delay = (int)json_get_number(json_find_member(mtcd,"coarse_delay"));
    mtc->mtcd.fine_delay = (int)json_get_number(json_find_member(mtcd,"fine_delay"));
    mtc->mtcd.gt_mask = strtoul(json_get_string(json_find_member(mtcd,"gt_mask")),(char**) NULL, 16);
    mtc->mtcd.gt_crate_mask = strtoul(json_get_string(json_find_member(mtcd,"gt_crate_mask")),(char**) NULL, 16);
    mtc->mtcd.ped_crate_mask = strtoul(json_get_string(json_find_member(mtcd,"ped_crate_mask")),(char**) NULL, 16);
    mtc->mtcd.control_mask = strtoul(json_get_string(json_find_member(mtcd,"control_mask")),(char**) NULL, 16);

    for (i=0;i<6;i++){
        sprintf(mtc->mtca.triggers[i].id,"%s",json_get_string(json_find_element(json_find_member(nhit,"id"),i)));
        mtc->mtca.triggers[i].threshold = (int)json_get_number(json_find_element(json_find_member(nhit,"threshold"),i));
        mtc->mtca.triggers[i].mv_per_adc = (int)json_get_number(json_find_element(json_find_member(nhit,"mv_per_adc"),i));
        mtc->mtca.triggers[i].mv_per_hit = (int)json_get_number(json_find_element(json_find_member(nhit,"mv_per_hit"),i));
        mtc->mtca.triggers[i].dc_offset = (int)json_get_number(json_find_element(json_find_member(nhit,"dc_offset"),i));
    }
    for (i=0;i<4;i++){
        sprintf(mtc->mtca.triggers[i+6].id,"%s",json_get_string(json_find_element(json_find_member(esum,"id"),i)));
        mtc->mtca.triggers[i+6].threshold = (int)json_get_number(json_find_element(json_find_member(esum,"threshold"),i));
        mtc->mtca.triggers[i+6].mv_per_adc = (int)json_get_number(json_find_element(json_find_member(esum,"mv_per_adc"),i));
        mtc->mtca.triggers[i+6].mv_per_hit = (int)json_get_number(json_find_element(json_find_member(esum,"mv_per_hit"),i));
        mtc->mtca.triggers[i+6].dc_offset = (int)json_get_number(json_find_element(json_find_member(esum,"dc_offset"),i));
    }
    for (i=0;i<4;i++){
        sprintf(mtc->mtca.triggers[i+10].id,"%s",json_get_string(json_find_element(json_find_member(spare,"id"),i)));
        mtc->mtca.triggers[i+10].threshold = (int)json_get_number(json_find_element(json_find_member(spare,"threshold"),i));
        mtc->mtca.triggers[i+10].mv_per_adc = (int)json_get_number(json_find_element(json_find_member(spare,"mv_per_adc"),i));
        mtc->mtca.triggers[i+10].mv_per_hit = (int)json_get_number(json_find_element(json_find_member(spare,"mv_per_hit"),i));
        mtc->mtca.triggers[i+10].dc_offset = (int)json_get_number(json_find_element(json_find_member(spare,"dc_offset"),i));
    }
    return 0;
}

int parse_test(JsonNode* test,mb_t* mb,int cbal,int zdisc,int all)
{
    int j,k;
    if (cbal == 1 || all == 1){
        for (j=0;j<2;j++){
            for (k=0;k<32;k++){
                mb->vbal[j][k] = (int)json_get_number(json_find_element(json_find_element(json_find_member(test,"vbal"),j),k)); 
            }
        }
    }
    if (zdisc == 1 || all == 1){
        for (k=0;k<32;k++){
            mb->vthr[k] = (int)json_get_number(json_find_element(json_find_member(test,"vthr"),k)); 
        }
    }
    return 0;
}

int parse_fec_debug(JsonNode* value,mb_t* mb)
{
    int j,k;
    for (j=0;j<2;j++){
        for (k=0;k<32;k++){
            mb->vbal[j][k] = (int)json_get_number(json_find_element(json_find_element(json_find_member(value,"vbal"),j),k)); 
            //printsend("[%d,%d] - %d\n",j,k,mb->vbal[j][k]);
        }
    }
    for (k=0;k<32;k++){
        mb->vthr[k] = (int)json_get_number(json_find_element(json_find_member(value,"vthr"),k)); 
        //printsend("[%d] - %d\n",k,mb->vthr[k]);
    }
    for (j=0;j<8;j++){
        mb->tdisc.rmp[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"rmp"),j));	
        mb->tdisc.rmpup[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"rmpup"),j));	
        mb->tdisc.vsi[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"vsi"),j));	
        mb->tdisc.vli[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"vli"),j));	
        //printsend("[%d] - %d %d %d %d\n",j,mb->tdisc.rmp[j],mb_consts->tdisc.rmpup[j],mb_consts->tdisc.vsi[j],mb_consts->tdisc.vli[j]);
    }
    mb->tcmos.vmax = (int)json_get_number(json_find_member(json_find_member(value,"tcmos"),"vmax"));
    mb->tcmos.tacref = (int)json_get_number(json_find_member(json_find_member(value,"tcmos"),"tacref"));
    //printsend("%d %d\n",mb->tcmos.vmax,mb_consts->tcmos.tacref);
    for (j=0;j<2;j++){
        mb->tcmos.isetm[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tcmos"),"isetm"),j));
        mb->tcmos.iseta[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tcmos"),"iseta"),j));
        //printsend("[%d] - %d %d\n",j,mb->tcmos.isetm[j],mb_consts->tcmos.iseta[j]);
    }
    for (j=0;j<32;j++){
        mb->tcmos.tac_shift[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tcmos"),"tac_shift"),j));
        //printsend("[%d] - %d\n",j,mb->tcmos.tac_shift[j]);
    }
    mb->vint = (int)json_get_number(json_find_member(value,"vint"));
    mb->hvref = (int)json_get_number(json_find_member(value,"hvref"));
    //printsend("%d %d\n",mb->vres,mb_consts->hvref);
    for (j=0;j<32;j++){
        mb->tr100.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr100"),"mask"),j));
        mb->tr100.tdelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr100"),"delay"),j));
        mb->tr20.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr20"),"mask"),j));
        mb->tr20.tdelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr20"),"delay"),j));
        mb->tr20.twidth[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr20"),"width"),j));
        //printsend("[%d] - %d %d %d %d %d\n",j,mb->tr100.mask[j],mb_consts->tr100.tdelay[j],mb_consts->tr20.tdelay[j],mb_consts->tr20.twidth[j]);
    }
    for (j=0;j<32;j++){
        mb->scmos[j] = (int)json_get_number(json_find_element(json_find_member(value,"scmos"),j));
        //printsend("[%d] - %d\n",j,mb->scmos[j]);
    }
    mb->disable_mask = 0x0;
    for (j=0;j<32;j++){
        if ((int)json_get_number(json_find_element(json_find_member(value,"chan_disable"),j)) != 0)
            mb->disable_mask |= (0x1<<j);
    }

    return 0;

}


int parse_fec_hw(JsonNode* value,mb_t* mb)
{
    int j,k;
    JsonNode* hw = json_find_member(value,"hw");
    mb->mb_id = strtoul(json_get_string(json_find_member(value,"board_id")),(char**)NULL,16);
    mb->dc_id[0] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db0")),(char**)NULL,16);
    mb->dc_id[1] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db1")),(char**)NULL,16);
    mb->dc_id[2] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db2")),(char**)NULL,16);
    mb->dc_id[3] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db3")),(char**)NULL,16);
    //printsend("%04x,%04x,%04x,%04x\n",mb->mb_id,mb_consts->dc_id[0],mb_consts->dc_id[1],mb_consts->dc_id[2],mb_consts->dc_id[3]);
    for (j=0;j<2;j++){
        for (k=0;k<32;k++){
            mb->vbal[j][k] = (int)json_get_number(json_find_element(json_find_element(json_find_member(hw,"vbal"),j),k)); 
            //printsend("[%d,%d] - %d\n",j,k,mb->vbal[j][k]);
        }
    }
    for (k=0;k<32;k++){
        mb->vthr[k] = (int)json_get_number(json_find_element(json_find_member(hw,"vthr"),k)); 
        //printsend("[%d] - %d\n",k,mb->vthr[k]);
    }
    for (j=0;j<8;j++){
        mb->tdisc.rmp[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"rmp"),j));	
        mb->tdisc.rmpup[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"rmpup"),j));	
        mb->tdisc.vsi[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"vsi"),j));	
        mb->tdisc.vli[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"vli"),j));	
        //printsend("[%d] - %d %d %d %d\n",j,mb->tdisc.rmp[j],mb_consts->tdisc.rmpup[j],mb_consts->tdisc.vsi[j],mb_consts->tdisc.vli[j]);
    }
    mb->tcmos.vmax = (int)json_get_number(json_find_member(json_find_member(hw,"tcmos"),"vmax"));
    mb->tcmos.tacref = (int)json_get_number(json_find_member(json_find_member(hw,"tcmos"),"tacref"));
    //printsend("%d %d\n",mb->tcmos.vmax,mb_consts->tcmos.tacref);
    for (j=0;j<2;j++){
        mb->tcmos.isetm[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tcmos"),"isetm"),j));
        mb->tcmos.iseta[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tcmos"),"iseta"),j));
        //printsend("[%d] - %d %d\n",j,mb->tcmos.isetm[j],mb_consts->tcmos.iseta[j]);
    }
    for (j=0;j<32;j++){
        mb->tcmos.tac_shift[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tcmos"),"tac_shift"),j));
        //printsend("[%d] - %d\n",j,mb->tcmos.tac_shift[j]);
    }
    mb->vint = (int)json_get_number(json_find_member(hw,"vint"));
    mb->hvref = (int)json_get_number(json_find_member(hw,"hvref"));
    //printsend("%d %d\n",mb->vres,mb_consts->hvref);
    for (j=0;j<32;j++){
        mb->tr100.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr100"),"mask"),j));
        mb->tr100.tdelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr100"),"delay"),j));
        mb->tr20.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr20"),"mask"),j));
        mb->tr20.tdelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr20"),"delay"),j));
        mb->tr20.twidth[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr20"),"width"),j));
        //printsend("[%d] - %d %d %d %d %d\n",j,mb->tr100.mask[j],mb_consts->tr100.tdelay[j],mb_consts->tr20.tdelay[j],mb_consts->tr20.twidth[j]);
    }
    for (j=0;j<32;j++){
        mb->scmos[j] = (int)json_get_number(json_find_element(json_find_member(hw,"scmos"),j));
        //printsend("[%d] - %d\n",j,mb->scmos[j]);
    }
    mb->disable_mask = 0x0;
    for (j=0;j<32;j++){
        if ((int)json_get_number(json_find_element(json_find_member(hw,"chan_disable"),j)) != 0)
            mb->disable_mask |= (0x1<<j);
    }
    return 0;

}

int swap_fec_db(mb_t* mb)
{
    SwapShortBlock(&(mb->mb_id),1);
    SwapShortBlock((mb->dc_id),4);
    SwapShortBlock((mb->scmos),32);
    SwapLongBlock(&(mb->disable_mask),1);
    return 0;
}

int post_debug_doc(int crate, int card, JsonNode* doc, fd_set *thread_fdset)
{
    char mb_ids[8],dc_ids[4][8];
    char put_db_address[500];
    update_crate_config(crate,0x1<<card,thread_fdset);
    time_t the_time;
    the_time = time(0); //
    char datetime[100];
    sprintf(datetime,"%s",(char *) ctime(&the_time));
    datetime[strlen(datetime)-1] = '\0';

    sprintf(mb_ids,"%04x",crate_config[crate][card].mb_id);
    sprintf(dc_ids[0],"%04x",crate_config[crate][card].dc_id[0]);
    sprintf(dc_ids[1],"%04x",crate_config[crate][card].dc_id[1]);
    sprintf(dc_ids[2],"%04x",crate_config[crate][card].dc_id[2]);
    sprintf(dc_ids[3],"%04x",crate_config[crate][card].dc_id[3]);
    
    JsonNode *config = json_mkobject();
    JsonNode *db = json_mkarray();
    int i;
    for (i=0;i<4;i++){
      JsonNode *db1 = json_mkobject();
      json_append_member(db1,"db_id",json_mkstring(dc_ids[i]));
      json_append_member(db1,"slot",json_mknumber((double)i));
      json_append_element(db,db1);
    }
    json_append_member(config,"db",db);
    json_append_member(config,"fec_id",json_mkstring(mb_ids));
    json_append_member(config,"crate_id",json_mknumber((double)crate));
    json_append_member(config,"slot",json_mknumber((double)card));
    if (CURRENT_LOCATION == PENN_TESTSTAND)
      json_append_member(config,"loc",json_mkstring("penn"));
    if (CURRENT_LOCATION == ABOVE_GROUND_TESTSTAND)
      json_append_member(config,"loc",json_mkstring("surface"));
    if (CURRENT_LOCATION == UNDERGROUND)
      json_append_member(config,"loc",json_mkstring("underground"));
    json_append_member(doc,"config",config);

    json_append_member(doc,"timestamp",json_mknumber((double)(long int) the_time));
    json_append_member(doc,"created",json_mkstring(datetime));

    // TODO: this might be leaking a lot...
    sprintf(put_db_address,"%s/%s",DB_SERVER,DB_BASE_NAME);
    pouch_request *post_response = pr_init();
    pr_set_method(post_response, POST);
    pr_set_url(post_response, put_db_address);
    char *data = json_encode(doc);
    pr_set_data(post_response, data);
    pr_do(post_response);
    int ret = 0;
    if (post_response->httpresponse != 201){
       pt_printsend("error code %d\n",(int)post_response->httpresponse);
        ret = -1;
    }
    pr_free(post_response);
    if(*data){
        free(data);
    }
    return ret;
};

int post_debug_doc_with_id(int crate, int card, char *id, JsonNode* doc, fd_set *thread_fdset)
{
    char mb_ids[8],dc_ids[4][8];
    char put_db_address[500];
    update_crate_config(crate,0x1<<card,thread_fdset);
    time_t the_time;
    the_time = time(0); //
    char datetime[100];
    sprintf(datetime,"%s",(char *) ctime(&the_time));
    datetime[strlen(datetime)-1] = '\0';

    sprintf(mb_ids,"%04x",crate_config[crate][card].mb_id);
    sprintf(dc_ids[0],"%04x",crate_config[crate][card].dc_id[0]);
    sprintf(dc_ids[1],"%04x",crate_config[crate][card].dc_id[1]);
    sprintf(dc_ids[2],"%04x",crate_config[crate][card].dc_id[2]);
    sprintf(dc_ids[3],"%04x",crate_config[crate][card].dc_id[3]);

    JsonNode *config = json_mkobject();
    JsonNode *db = json_mkarray();
    int i;
    for (i=0;i<4;i++){
      JsonNode *db1 = json_mkobject();
      json_append_member(db1,"db_id",json_mkstring(dc_ids[i]));
      json_append_member(db1,"slot",json_mknumber((double)i));
      json_append_element(db,db1);
    }
    json_append_member(config,"db",db);
    json_append_member(config,"fec_id",json_mkstring(mb_ids));
    json_append_member(config,"crate_id",json_mknumber((double)crate));
    json_append_member(config,"slot",json_mknumber((double)card));
    if (CURRENT_LOCATION == PENN_TESTSTAND)
      json_append_member(config,"loc",json_mkstring("penn"));
    if (CURRENT_LOCATION == ABOVE_GROUND_TESTSTAND)
      json_append_member(config,"loc",json_mkstring("surface"));
    if (CURRENT_LOCATION == UNDERGROUND)
      json_append_member(config,"loc",json_mkstring("underground"));
    json_append_member(doc,"config",config);

    json_append_member(doc,"timestamp",json_mknumber((double)(long int) the_time));
    json_append_member(doc,"created",json_mkstring(datetime));

    sprintf(put_db_address,"%s/%s/%s",DB_SERVER,DB_BASE_NAME,id);
    pouch_request *post_response = pr_init();
    pr_set_method(post_response, PUT);
    pr_set_url(post_response, put_db_address);
    char *data = json_encode(doc);
    pr_set_data(post_response, data);
    pr_do(post_response);
    int ret = 0;
    if (post_response->httpresponse != 201){
      pt_printsend("error code %d\n",(int)post_response->httpresponse);
      ret = -1;
    }
    pr_free(post_response);
    if(*data){
      free(data);
    }
    return 0;
};

int post_debug_doc_mem_test(int crate, int card, JsonNode* doc, fd_set *thread_fdset)
{
  char mb_ids[8],dc_ids[4][8];
  char put_db_address[500];
  time_t the_time;
  the_time = time(0); //
  char datetime[100];
  sprintf(datetime,"%s",(char *) ctime(&the_time));
  datetime[strlen(datetime)-1] = '\0';

  sprintf(mb_ids,"%04x",crate_config[crate][card].mb_id);
  sprintf(dc_ids[0],"%04x",crate_config[crate][card].dc_id[0]);
  sprintf(dc_ids[1],"%04x",crate_config[crate][card].dc_id[1]);
  sprintf(dc_ids[2],"%04x",crate_config[crate][card].dc_id[2]);
  sprintf(dc_ids[3],"%04x",crate_config[crate][card].dc_id[3]);

  JsonNode *config = json_mkobject();
  JsonNode *db = json_mkarray();
  int i;
  for (i=0;i<4;i++){
    JsonNode *db1 = json_mkobject();
    json_append_member(db1,"db_id",json_mkstring(dc_ids[i]));
    json_append_member(db1,"slot",json_mknumber((double)i));
    json_append_element(db,db1);
  }
  json_append_member(config,"db",db);
  json_append_member(config,"fec_id",json_mkstring(mb_ids));
  json_append_member(config,"crate_id",json_mknumber((double)crate));
  json_append_member(config,"slot",json_mknumber((double)card));
  if (CURRENT_LOCATION == PENN_TESTSTAND)
    json_append_member(config,"loc",json_mkstring("penn"));
  if (CURRENT_LOCATION == ABOVE_GROUND_TESTSTAND)
    json_append_member(config,"loc",json_mkstring("surface"));
  if (CURRENT_LOCATION == UNDERGROUND)
    json_append_member(config,"loc",json_mkstring("underground"));
  json_append_member(doc,"config",config);

  json_append_member(doc,"timestamp",json_mknumber((double)(long int) the_time));
  json_append_member(doc,"created",json_mkstring(datetime));

  // TODO: this might be leaking a lot...
  sprintf(put_db_address,"%s/%s",DB_SERVER,DB_BASE_NAME);
  pouch_request *post_response = pr_init();
  pr_set_method(post_response, POST);
  pr_set_url(post_response, put_db_address);
  char *data = json_encode(doc);
  pr_set_data(post_response, data);
  pr_do(post_response);
  int ret = 0;
  if (post_response->httpresponse != 201){
    pt_printsend("error code %d\n",(int)post_response->httpresponse);
    ret = -1;
  }
  pr_free(post_response);
  if(*data){
    free(data);
  }
  printf("exiting\n");
  return ret;
};

int post_ecal_doc(uint32_t crate_mask, uint16_t *slot_mask, char *logfile, char *id)
{
  JsonNode *doc = json_mkobject();
  time_t the_time;
  the_time = time(0); //
  char datetime[100];
  sprintf(datetime,"%s",(char *) ctime(&the_time));
  datetime[strlen(datetime)-1] = '\0';

  char masks[8];

  JsonNode *crates = json_mkarray();
  int i;
  for (i=0;i<19;i++){
    JsonNode *one_crate = json_mkobject();
    if ((0x1<<i) & crate_mask){
      sprintf(masks,"%04x",slot_mask[i]);
    }else{
      sprintf(masks,"0000");
    }
    json_append_member(one_crate,"slot_mask",json_mkstring(masks));
    json_append_member(one_crate,"crate_id",json_mknumber(i));
    json_append_element(crates,one_crate);
  }
  json_append_member(doc,"crate_slot_masks",crates);
  json_append_member(doc,"logfile_name",json_mkstring(logfile));
  json_append_member(doc,"type",json_mkstring("ecal"));
  json_append_member(doc,"timestamp",json_mknumber((double)(long int) the_time));
  json_append_member(doc,"created",json_mkstring(datetime));

  char put_db_address[500];
  sprintf(put_db_address,"%s/%s/%s",DB_SERVER,DB_BASE_NAME,id);
  pouch_request *post_response = pr_init();
  pr_set_method(post_response, PUT);
  pr_set_url(post_response, put_db_address);
  char *data = json_encode(doc);
  pr_set_data(post_response, data);
  pr_do(post_response);
  int ret = 0;
  if (post_response->httpresponse != 201){
    pt_printsend("error code %d\n",(int)post_response->httpresponse);
    ret = -1;
  }
  pr_free(post_response);
  json_delete(doc);
  if(*data){
    free(data);
  }
  return 0;
};


