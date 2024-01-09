#pragma once
#include <string>
#include"xiling_player.h"
#include"_log.h"


struct jineng {
	std::string m_name;
	int m_bianhao;
	pinzhi m_pinzhi;
	xiling_player* m_whohave;

	bool m_incd;
	int m_cdmax;

	jineng()
		:m_name(""),m_bianhao(0),m_pinzhi(white),m_whohave(nullptr),m_incd(0),m_cdmax(60) {}
	jineng(std::string name,int bianhao,pinzhi pinzhi,xiling_player* who,int cdmax)
		:m_name(name),m_bianhao(bianhao),m_pinzhi(pinzhi),m_whohave(who),m_incd(0),m_cdmax(cdmax) {}
	jineng(const jineng& one,xiling_player* who)
		:m_name(one.m_name),m_bianhao(one.m_bianhao),m_pinzhi(one.m_pinzhi),m_whohave(who),m_incd(one.m_incd),m_cdmax(one.m_cdmax){}
	//jineng(const jineng& other) = delete;
	//jineng(jineng&& other) = delete;
	
	

	std::string send(std::string(*fadong)(std::vector<xiling_player>& _7fang, short userid), std::vector<xiling_player>& _7fang, short userid) {
		return fadong(_7fang, userid);
	}
	std::string send(std::string(*fadong)(std::vector<xiling_player>& _7fang, short userid, short useto), std::vector<xiling_player>& _7fang, short userid, short useto) {
		return fadong(_7fang, userid, useto);
	}
	std::string(*find_jineng_fadong1(int bianhao))(std::vector<xiling_player>& _7fang, short userid);
	std::string(*find_jineng_fadong2(int bianhao))(std::vector<xiling_player>& _7fang, short userid,short useto);


	void incd();

};



void chixu_cd(bool* incd, int cdmax);


//7房，专属技
std::string fadong_taixujianshen(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_taixujianshen(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string fadong_jubaoranhun(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_jubaoranhun(std::vector<xiling_player>& _7fang, short userid, short useto);
void chixu_qianhuizhuanshu(std::vector<xiling_player>* _7fang);


//通用技
std::string fadong_putonggongji(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_putonggongji(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string fadong_molichongji(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_molichongji(std::vector<xiling_player>& _7fang, short userid, short useto);

std::string fadong_meihuo(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_meihuo(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string fadong_naonaonao(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_naonaonao(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string fadong_bingdun(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_bingdun(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string fadong_zhongchui(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_zhongchui(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string fadong_shanguangdan(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_shanguangdan(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string fadong_zhiliaoshu(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_zhiliaoshu(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string fadong_huoqiushu(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_huoqiushu(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string fadong_huanshounaqiu(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_huanshounaqiu(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string fadong_baonue(std::vector<xiling_player>& _7fang, short userid);
std::string fadong_baonue(std::vector<xiling_player>& _7fang, short userid, short useto);
void chixu_baonue(xiling_player* player);



