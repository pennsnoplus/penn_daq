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
    mb->db_id[0] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db0")),(char**)NULL,16);
    mb->db_id[1] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db1")),(char**)NULL,16);
    mb->db_id[2] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db2")),(char**)NULL,16);
    mb->db_id[3] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db3")),(char**)NULL,16);
    //printsend("%04x,%04x,%04x,%04x\n",mb->mb_id,mb_consts->db_id[0],mb_consts->db_id[1],mb_consts->db_id[2],mb_consts->db_id[3]);
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
    SwapShortBlock((mb->db_id),4);
    SwapShortBlock((mb->scmos),32);
    SwapLongBlock(&(mb->disable_mask),1);
    return 0;
}

int create_fec_db_doc(int crate, int card, JsonNode** doc_p, JsonNode *ecal_doc, fd_set *thread_fdset)
{
  int i,j;
  // lets pull out what we need from the configuration document
  JsonNode *time_stamp = json_find_member(ecal_doc,"formatted_timestamp");
  JsonNode *config;
  JsonNode *crates = json_find_member(ecal_doc,"config");
  for (i=0;i<json_get_num_mems(crates);i++){
    JsonNode *one_crate = json_find_element(crates,i);
    if (json_get_number(json_find_member(one_crate,"crate_id")) == crate){
      JsonNode *slots = json_find_member(one_crate,"slots");
      for (j=0;j<json_get_num_mems(slots);j++){
        JsonNode *one_slot = json_find_element(slots,j);
        if (json_get_number(json_find_member(one_slot,"slot_id")) == card){
          config = one_slot;
          break;
        }
      }
    }
  }
  JsonNode *settings = json_find_member(ecal_doc,"settings");

  JsonNode *doc = json_mkobject();

  json_append_member(doc,"name",json_mkstring("FEC"));
  json_append_member(doc,"crate",json_mknumber(crate));
  json_append_member(doc,"card",json_mknumber(card));


  json_append_member(doc,"time_stamp",json_mkstring(json_get_string(time_stamp)));
  json_append_member(doc,"approved",json_mkbool(0));

  json_append_member(doc,"board_id",json_mkstring(json_get_string(json_find_member(config,"mb_id"))));
  
  JsonNode *hw = json_mkobject();
  json_append_member(hw,"vint",json_mknumber(json_get_number(json_find_member(settings,"vint"))));
  json_append_member(hw,"hvref",json_mknumber(json_get_number(json_find_member(settings,"hvref"))));
  json_append_member(hw,"tr100",json_mkcopy(json_find_member(settings,"tr100")));
  json_append_member(hw,"tr20",json_mkcopy(json_find_member(settings,"tr20")));
  json_append_member(hw,"scmos",json_mkcopy(json_find_member(settings,"scmos")));
  json_append_member(doc,"hw",hw);
  JsonNode *id = json_mkobject();
  json_append_member(id,"db0",json_mkstring(json_get_string(json_find_member(config,"db0_id"))));
  json_append_member(id,"db1",json_mkstring(json_get_string(json_find_member(config,"db1_id"))));
  json_append_member(id,"db2",json_mkstring(json_get_string(json_find_member(config,"db2_id"))));
  json_append_member(id,"db3",json_mkstring(json_get_string(json_find_member(config,"db3_id"))));
  json_append_member(id,"hv",json_mkstring("0x0000")); //FIXME
  json_append_member(doc,"id",id);

  JsonNode *test = json_mkobject();
  json_append_member(doc,"test",test);

  JsonNode *tube = json_mkobject();
  JsonNode *problem = json_mkarray();
  for (i=0;i<32;i++){
    json_append_element(problem,json_mknumber(0)); //FIXME
  }
  json_append_member(tube,"problem",problem);
  json_append_member(tube,"cable_link",json_mkstring("0")); //FIXME
  json_append_member(doc,"tube",tube);

  JsonNode *comment = json_mkarray();
  json_append_member(doc,"comment",comment);

  *doc_p = doc;

  return 0;
}


int add_ecal_test_results(JsonNode *fec_doc, JsonNode *test_doc)
{
  int i,j;
  char type[50];
  JsonNode *hw = json_find_member(fec_doc,"hw");
  JsonNode *test = json_find_member(fec_doc,"test");
  sprintf(type,"%s",json_get_string(json_find_member(test_doc,"type")));
  JsonNode *test_entry = json_mkobject();
  json_append_member(test_entry,"test_id",json_mkstring(json_get_string(json_find_member(test_doc,"_id"))));
  json_append_member(test,type,test_entry);

  if (strcmp(type,"crate_cbal") == 0){
    JsonNode *vbal = json_mkarray();
    JsonNode *high = json_mkarray();
    JsonNode *low = json_mkarray();
    JsonNode *channels = json_find_member(test_doc,"channels");
    for (i=0;i<32;i++){
      JsonNode *one_chan = json_find_element(channels,i);
      json_append_element(high,json_mknumber(json_get_number(json_find_member(one_chan,"vbal_high"))));
      json_append_element(low,json_mknumber(json_get_number(json_find_member(one_chan,"vbal_low"))));
    }
    json_append_element(vbal,high);
    json_append_element(vbal,low);
    json_append_member(hw,"vbal",vbal);
  }else if (strcmp(type,"zdisc") == 0){
    JsonNode *vthr_zero = json_mkarray();
    JsonNode *vals = json_find_member(test_doc,"zero_dac");
    for (i=0;i<32;i++){
      json_append_element(vthr_zero,json_mknumber(json_get_number(json_find_element(vals,i))));
    }
    json_append_member(hw,"vthr_zero",vthr_zero);
  }else if (strcmp(type,"set_ttot") == 0){
   JsonNode *tdisc = json_mkobject();
   JsonNode *rmp = json_mkarray();
   JsonNode *rmpup = json_mkarray();
   JsonNode *vsi = json_mkarray();
   JsonNode *vli = json_mkarray();
   JsonNode *chips = json_find_member(test_doc,"chips");
   for (i=0;i<8;i++){
     JsonNode *one_chip = json_find_element(chips,i);
     json_append_element(rmp,json_mknumber(json_get_number(json_find_member(one_chip,"rmp"))));
     json_append_element(vsi,json_mknumber(json_get_number(json_find_member(one_chip,"vsi"))));
     json_append_element(rmpup,json_mknumber(115)); //FIXME`
     json_append_element(vli,json_mknumber(120)); //FIXME`
   }
   json_append_member(tdisc,"rmp",rmp);
   json_append_member(tdisc,"rmpup",rmpup);
   json_append_member(tdisc,"vsi",vsi);
   json_append_member(tdisc,"vli",vli);
   json_append_member(hw,"tdisc",tdisc);
  }else if (strcmp(type,"cmos_m_gtvalid") == 0){
    JsonNode *tcmos = json_mkobject();
    json_append_member(tcmos,"vmax",json_mknumber(json_get_number(json_find_member(test_doc,"vmax"))));
    json_append_member(tcmos,"vtacref",json_mknumber(json_get_number(json_find_member(test_doc,"tacref"))));
    JsonNode *isetm_vals = json_find_member(test_doc,"isetm");
    JsonNode *iseta_vals = json_find_member(test_doc,"iseta");
    JsonNode *isetm = json_mkarray();
    JsonNode *iseta = json_mkarray();
    for (i=0;i<2;i++){
      json_append_element(isetm,json_mknumber(json_get_number(json_find_element(isetm_vals,i))));
      json_append_element(iseta,json_mknumber(json_get_number(json_find_element(iseta_vals,i))));
    }

    json_append_member(tcmos,"isetm",isetm);
    json_append_member(tcmos,"iseta",iseta);
    JsonNode *channels = json_find_member(test_doc,"channels");
    JsonNode *tac_trim = json_mkarray();
    for (i=0;i<32;i++){
      JsonNode *one_chan = json_find_element(channels,i);
      json_append_element(tac_trim,json_mknumber(json_get_number(json_find_member(one_chan,"tac_shift"))));
    }
    json_append_member(tcmos,"tac_trim",tac_trim);
    json_append_member(hw,"tcmos",tcmos);
  }else if (strcmp(type,"find_noise") == 0){
    JsonNode *vthr = json_mkarray();
    JsonNode *channels = json_find_member(test_doc,"channels");
    for (i=0;i<32;i++){
      JsonNode *one_chan = json_find_element(channels,i);
      uint32_t zero_used = json_get_number(json_find_member(one_chan,"zero_used"));
      JsonNode *points = json_find_member(one_chan,"points");
      int total_rows = json_get_num_mems(points); 
      JsonNode *final_point = json_find_element(points,total_rows-1);
      uint32_t readout_dac = json_get_number(json_find_member(final_point,"thresh_above_zero"));
      json_append_element(vthr,json_mknumber(zero_used+readout_dac));
    }
    json_append_member(hw,"vthr",vthr);
  }
  return 0;
}

int post_fec_db_doc(int crate, int slot, JsonNode *doc){
  char put_db_address[500];
  sprintf(put_db_address,"%s/%s",FECDB_SERVER,FECDB_BASE_NAME);
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
}

int update_fec_db_doc(JsonNode *doc)
{
  char put_db_address[500];
  sprintf(put_db_address,"%s/%s/%s",FECDB_SERVER,FECDB_BASE_NAME,json_get_string(json_find_member(doc,"_id")));
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
}

int post_debug_doc(int crate, int card, JsonNode* doc, fd_set *thread_fdset)
{
  char mb_id[8],db_id[4][8];
  char put_db_address[500];
  update_crate_config(crate,0x1<<card,thread_fdset);
  time_t the_time;
  the_time = time(0); //
  char datetime[100];
  sprintf(datetime,"%s",(char *) ctime(&the_time));
  datetime[strlen(datetime)-1] = '\0';

  sprintf(mb_id,"%04x",crate_config[crate][card].mb_id);
  sprintf(db_id[0],"%04x",crate_config[crate][card].db_id[0]);
  sprintf(db_id[1],"%04x",crate_config[crate][card].db_id[1]);
  sprintf(db_id[2],"%04x",crate_config[crate][card].db_id[2]);
  sprintf(db_id[3],"%04x",crate_config[crate][card].db_id[3]);

  JsonNode *config = json_mkobject();
  JsonNode *db = json_mkarray();
  int i;
  for (i=0;i<4;i++){
    JsonNode *db1 = json_mkobject();
    json_append_member(db1,"db_id",json_mkstring(db_id[i]));
    json_append_member(db1,"slot",json_mknumber((double)i));
    json_append_element(db,db1);
  }
  json_append_member(config,"db",db);
  json_append_member(config,"fec_id",json_mkstring(mb_id));
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
  char mb_id[8],db_id[4][8];
  char put_db_address[500];
  update_crate_config(crate,0x1<<card,thread_fdset);
  time_t the_time;
  the_time = time(0); //
  char datetime[100];
  sprintf(datetime,"%s",(char *) ctime(&the_time));
  datetime[strlen(datetime)-1] = '\0';

  sprintf(mb_id,"%04x",crate_config[crate][card].mb_id);
  sprintf(db_id[0],"%04x",crate_config[crate][card].db_id[0]);
  sprintf(db_id[1],"%04x",crate_config[crate][card].db_id[1]);
  sprintf(db_id[2],"%04x",crate_config[crate][card].db_id[2]);
  sprintf(db_id[3],"%04x",crate_config[crate][card].db_id[3]);

  JsonNode *config = json_mkobject();
  JsonNode *db = json_mkarray();
  int i;
  for (i=0;i<4;i++){
    JsonNode *db1 = json_mkobject();
    json_append_member(db1,"db_id",json_mkstring(db_id[i]));
    json_append_member(db1,"slot",json_mknumber((double)i));
    json_append_element(db,db1);
  }
  json_append_member(config,"db",db);
  json_append_member(config,"fec_id",json_mkstring(mb_id));
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
  char mb_id[8],db_id[4][8];
  char put_db_address[500];
  time_t the_time;
  the_time = time(0); //
  char datetime[100];
  sprintf(datetime,"%s",(char *) ctime(&the_time));
  datetime[strlen(datetime)-1] = '\0';

  sprintf(mb_id,"%04x",crate_config[crate][card].mb_id);
  sprintf(db_id[0],"%04x",crate_config[crate][card].db_id[0]);
  sprintf(db_id[1],"%04x",crate_config[crate][card].db_id[1]);
  sprintf(db_id[2],"%04x",crate_config[crate][card].db_id[2]);
  sprintf(db_id[3],"%04x",crate_config[crate][card].db_id[3]);

  JsonNode *config = json_mkobject();
  JsonNode *db = json_mkarray();
  int i;
  for (i=0;i<4;i++){
    JsonNode *db1 = json_mkobject();
    json_append_member(db1,"db_id",json_mkstring(db_id[i]));
    json_append_member(db1,"slot",json_mknumber((double)i));
    json_append_element(db,db1);
  }
  json_append_member(config,"db",db);
  json_append_member(config,"fec_id",json_mkstring(mb_id));
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

int post_ecal_doc(uint32_t crate_mask, uint16_t *slot_mask, char *logfile, char *id, fd_set *thread_fdset)
{
  JsonNode *doc = json_mkobject();
  printf("in post ecal\n");

  // lets get what we need from the current crate init doc
  char get_db_address[500];
  sprintf(get_db_address,"%s/%s/CRATE_INIT_DOC",DB_SERVER,DB_BASE_NAME);
  pouch_request *init_response = pr_init();
  pr_set_method(init_response, GET);
  pr_set_url(init_response, get_db_address);
  printf("do\n");
  pr_do(init_response);
  printf("did\n");
  if (init_response->httpresponse != 200){
    pt_printsend("Unable to connect to database. error code %d\n",(int)init_response->httpresponse);
    return;
  }
  JsonNode *init_doc = json_decode(init_response->resp.data);
  JsonNode *settings = json_mkobject();
  json_append_member(settings,"vint",json_mknumber(json_get_number(json_find_member(init_doc,"vint"))));
  json_append_member(settings,"hvref",json_mknumber(json_get_number(json_find_member(init_doc,"hvref"))));
  json_append_member(settings,"tr100",json_mkcopy(json_find_member(init_doc,"tr100")));
  json_append_member(settings,"tr20",json_mkcopy(json_find_member(init_doc,"tr20")));
  json_append_member(settings,"scmos",json_mkcopy(json_find_member(init_doc,"scmos")));
  json_append_member(doc,"settings",settings);

  time_t the_time;
  the_time = time(0); //
  char datetime[100];
  sprintf(datetime,"%s",(char *) ctime(&the_time));
  datetime[strlen(datetime)-1] = '\0';

  char masks[8];

  int i,j;
  JsonNode *crates = json_mkarray();
  for (i=0;i<19;i++){
    if ((0x1<<i) & crate_mask){
      JsonNode *one_crate = json_mkobject();
      json_append_member(one_crate,"crate_id",json_mknumber(i));
      sprintf(masks,"%04x",slot_mask[i]);
      json_append_member(one_crate,"slot_mask",json_mkstring(masks));

      JsonNode *slots = json_mkarray();
      for (j=0;j<16;j++){
        if ((0x1<<j) & slot_mask[i]){
          char mb_id[8],db_id[4][8];
          char put_db_address[500];
          printf("updating crate config for %d, %d\n",i,j);
          update_crate_config(i,0x1<<j,thread_fdset);
          printf("updated\n");

          sprintf(mb_id,"%04x",crate_config[i][j].mb_id);
          sprintf(db_id[0],"%04x",crate_config[i][j].db_id[0]);
          sprintf(db_id[1],"%04x",crate_config[i][j].db_id[1]);
          sprintf(db_id[2],"%04x",crate_config[i][j].db_id[2]);
          sprintf(db_id[3],"%04x",crate_config[i][j].db_id[3]);

          JsonNode *one_slot = json_mkobject();
          json_append_member(one_slot,"slot_id",json_mknumber(j));
          json_append_member(one_slot,"mb_id",json_mkstring(mb_id));
          json_append_member(one_slot,"db0_id",json_mkstring(db_id[0]));
          json_append_member(one_slot,"db1_id",json_mkstring(db_id[1]));
          json_append_member(one_slot,"db2_id",json_mkstring(db_id[2]));
          json_append_member(one_slot,"db3_id",json_mkstring(db_id[3]));
          json_append_element(slots,one_slot);
        }
      }
      json_append_member(one_crate,"slots",slots);

      json_append_element(crates,one_crate);
    }
  }

  json_append_member(doc,"config",crates);
  json_append_member(doc,"logfile_name",json_mkstring(logfile));
  json_append_member(doc,"type",json_mkstring("ecal"));
  json_append_member(doc,"timestamp",json_mknumber((double)(long int) the_time));
  json_append_member(doc,"created",json_mkstring(datetime));


  time_t curtime = time(NULL);
  struct tm *loctime = localtime(&curtime);
  char time_stamp[500];
  strftime(time_stamp,256,"%Y-%m-%dT%H:%M:%S",loctime);
  json_append_member(doc,"formatted_timestamp",json_mkstring(time_stamp));

  char put_db_address[500];
  sprintf(put_db_address,"%s/%s/%s",DB_SERVER,DB_BASE_NAME,id);
  pouch_request *post_response = pr_init();
  pr_set_method(post_response, PUT);
  pr_set_url(post_response, put_db_address);
  char *data = json_encode(doc);
  pr_set_data(post_response, data);
  printf("about to post\n");
  pr_do(post_response);
  printf("posted\n");
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


