#include"xiling_player.h"
#include"_log.h"
#include<vector>
#include<thread>

struct wupin {
	std::string m_name;
	int m_bianhao;//编号
	pinzhi m_pinzhi;//品质：白蓝紫红
	//int m_price;//价值，金币价值，商店售价
	xiling_player* m_whohave;//持有人

	int m_naijiu;//耐久度，部分物品拥有，其他一次性物品耐久度值为0

	wupin()
		:m_name(""), m_bianhao(0), m_pinzhi(white), m_whohave(nullptr),m_naijiu(0)
	{}
	wupin(std::string thename, int duiying, pinzhi pinzhi, xiling_player* who)
		:m_name(thename), m_bianhao(duiying), m_pinzhi(pinzhi), m_whohave(who), m_naijiu(0)
	{}
	wupin(std::string thename, int duiying, pinzhi pinzhi, xiling_player* who,int naijiu)
		:m_name(thename), m_bianhao(duiying), m_pinzhi(pinzhi), m_whohave(who),m_naijiu(naijiu)
	{}
	wupin(const wupin& one,xiling_player* who)
		:m_name(one.m_name),m_bianhao(one.m_bianhao),m_pinzhi(one.m_pinzhi),m_whohave(who),m_naijiu(one.m_naijiu) 
	{}
	//wupin(const wupin& one) = delete;

	void give(xiling_player& other) {
		
		for (int i = 0; i < m_whohave->beibao.size();i++) {
			if (m_whohave->beibao[i]->m_bianhao == this->m_bianhao) {
				other.beibao.emplace_back(std::make_unique<wupin>(*m_whohave->beibao[i],&other));
				m_whohave->beibao.erase(m_whohave->beibao.begin() + i);
				break;
			}
		}
	}
	
	bool operator==(const wupin& other) {
		if (m_bianhao == other.m_bianhao)return 1;
		return 0;
	}
	/*bool operator>(const wupin& other) {
		if (m_pinzhi > other.m_pinzhi) {
			return 1;
		}
		else if (m_pinzhi == other.m_pinzhi) {
			if (m_price > other.m_price)return 1;
			else return 0;
		}
		else return 0;
	}
	bool operator<(const wupin& other) {
		return *this > other ? 0 : 1;
	}
	int operator+(const wupin& other) {
		return this->m_price + other.m_price;
	}*/
	//void operator=(wupin& one) {
	//	this->m_bianhao = one.m_bianhao;
	//	this->m_name = one.m_name;
	//	this->m_pinzhi = one.m_pinzhi;
	//	//this->m_price = one.m_price;
	//	this->m_whohave = one.m_whohave;
	//}
	void operator()(wupin& one, xiling_player& sb) {
		(*this) = one;
		this->m_whohave = &sb;
	}

	std::string getstring(pinzhi& one) {
		switch (one) {
		case 0:return "白";
		case 1:return "蓝";
		case 2:return "紫";
		case 3:return "金";
		default:
			return "[error] pinzhi 越界";
		}
	}

	std::string show(std::string(*show)()) {
		return show();
	}

	std::string send(std::string(*use)(std::vector<xiling_player>&, short), std::vector<xiling_player>& _7fang, short userid) {
		return use(_7fang, userid);
	}
	std::string send(std::string(*use)(std::vector<xiling_player>&, short, short), std::vector<xiling_player>& _7fang, short userid, short useto) {
		return use(_7fang, userid, useto);
	}
	static std::string(*find_wupin_1use(int bianhao))(std::vector<xiling_player>&, short);
	static std::string(*find_wupin_2use(int bianhao))(std::vector<xiling_player>&, short, short);
};

#pragma region 没用的东西，旧的物品类设计

//
//class wuxiaoguo : public wupin {
//	std::string m_xiaoguo;
//public:
//
//	virtual std::string show() = 0;
//	virtual void use() = 0;
//};
//class zengyi :public wupin {
//
//public:
//	virtual std::string show() = 0;
//	virtual void use() = 0;
//	virtual void use(xiling_player& other) = 0;
//};
//class haogandu :public wupin {
//	int m_haogan;
//
//public:
//	virtual std::string show() = 0;
//	virtual void use(xiling_player& other) = 0;
//};
//class jinengshu : public wupin {
//	int m_bianhao_jineng;
//
//public:
//	virtual std::string show() = 0;
//	virtual void use() = 0;
//};
//class shuijinqiu : public wupin {
//	
//public:
//	virtual std::string show() = 0;
//	virtual void use() = 0;
//};


#pragma endregion

struct zhuangtai_chixu {
	bool m_isExist;
	short m_id;

	zhuangtai_chixu(bool isExist,short id)
		:m_isExist(isExist),m_id(id){}
	~zhuangtai_chixu() {
		_log<errorlevel>(green, "持续状态结束。").m_log();
	}
};

void chixu_addhealth(std::vector<xiling_player>* _7fang, short id);
void chixu_addmagic(std::vector<xiling_player>* _7fang, short id);
void zhuangtai_10min(std::vector<xiling_player>* _7fang, short id, zhuangtai buff);

void zhuangtai_always(std::vector<xiling_player>& _7fang, short id, zhuangtai onebuff);
void zhuangtai_erase(std::vector<xiling_player>& _7fang, short id, zhuangtai onebuff);

std::string use_pingguo(std::vector<xiling_player>& _7fang, short userid);
std::string use_pingguo(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_qiguaideqiu(std::vector<xiling_player>& _7fang, short userid);
std::string use_qiguaideqiu(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_shuiqiang(std::vector<xiling_player>& _7fang, short userid);
std::string use_shuiqiang(std::vector<xiling_player>& _7fang, short userid, short useto);

std::string use_xiaoyaoping(std::vector<xiling_player>& _7fang, short userid);
std::string use_xiaoyaoping(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_zhongyaoping(std::vector<xiling_player>& _7fang, short userid);
std::string use_zhongyaoping(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_dayaoping(std::vector<xiling_player>& _7fang, short userid);
std::string use_dayaoping(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_xiaomoliping(std::vector<xiling_player>& _7fang, short userid);
std::string use_xiaomoliping(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_zhongmoliping(std::vector<xiling_player>& _7fang, short userid);
std::string use_zhongmoliping(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_damoliping(std::vector<xiling_player>& _7fang, short userid);
std::string use_damoliping(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_daliwan(std::vector<xiling_player>& _7fang, short userid);
std::string use_daliwan(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_xuruoji(std::vector<xiling_player>& _7fang, short userid);
std::string use_xuruoji(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_diannaopeijian(std::vector<xiling_player>& _7fang, short userid);
std::string use_diannaopeijian(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_tangsengrou(std::vector<xiling_player>& _7fang, short userid);
std::string use_tangsengrou(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_xiaoduwan(std::vector<xiling_player>& _7fang, short userid);
std::string use_xiaoduwan(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_jieduwan(std::vector<xiling_player>& _7fang, short userid);
std::string use_jieduwan(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_hongcha(std::vector<xiling_player>& _7fang, short userid);
std::string use_hongcha(std::vector<xiling_player>& _7fang, short userid, short useto);
std::string use_fushengdan(std::vector<xiling_player>& _7fang, short userid);
std::string use_fushengdan(std::vector<xiling_player>& _7fang, short userid, short useto);

std::string use_meihuo(std::vector<xiling_player>& _7fang, short userid);
std::string use_meihuo(std::vector<xiling_player>& _7fang, short userid,short useto);
std::string use_naonaonao(std::vector<xiling_player>& _7fang, short userid);
std::string use_naonaonao(std::vector<xiling_player>& _7fang, short userid,short useto);
std::string use_bingdun(std::vector<xiling_player>& _7fang, short userid);
std::string use_bingdun(std::vector<xiling_player>& _7fang, short userid,short useto);
std::string use_zhiliaoshu(std::vector<xiling_player>& _7fang, short userid);
std::string use_zhiliaoshu(std::vector<xiling_player>& _7fang, short userid,short useto);
std::string use_huoqiushu(std::vector<xiling_player>& _7fang, short userid);
std::string use_huoqiushu(std::vector<xiling_player>& _7fang, short userid,short useto);
std::string use_huanshounaqiu(std::vector<xiling_player>& _7fang, short userid);
std::string use_huanshounaqiu(std::vector<xiling_player>& _7fang, short userid,short useto);
std::string use_zhongchui(std::vector<xiling_player>& _7fang, short userid);
std::string use_zhongchui(std::vector<xiling_player>& _7fang, short userid,short useto);
std::string use_shanguangdan(std::vector<xiling_player>& _7fang, short userid);
std::string use_shanguangdan(std::vector<xiling_player>& _7fang, short userid,short useto);
std::string use_baonue(std::vector<xiling_player>& _7fang, short userid);
std::string use_baonue(std::vector<xiling_player>& _7fang, short userid,short useto);


std::string use_huibuliuqiushuijinqiu1(std::vector<xiling_player>& _7fang, short userid);
std::string use_huibuliuqiushuijinqiu1(std::vector<xiling_player>& _7fang, short userid,short useto);

std::string use_null(std::vector<xiling_player>& _7fang, short userid);
std::string use_null(std::vector<xiling_player>& _7fang, short userid, short useto);










//自定义
//template<typename T = std::string>
//std::string use_wuxiaoguo_zidingyi(T ,short userid, short useto, short health, short magic,int jinbi) {
//
//	return "还没做出来";
//}






