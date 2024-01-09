#pragma once

#include <string>
#include <vector>
#include<memory>

struct wupin;
struct jineng;
class xiling_player;

//typedef std::string(*wupin_1use)(std::vector<xiling_player>&, short);
//typedef std::string(*wupin_2use)(std::vector<xiling_player>&, short, short);

enum laohuji_yuansu :unsigned char {
	gundong = 0,
	pingguo = 1,
	mangguo = 1 << 1,
	lindang = 1 << 2,
	xigua = 1 << 3,
	xingxing = 1 << 4,
	qiqi = 1 << 5,
	baibian = 1 | 2 | 4 | 8 | 16 | 32 | 64
};

enum pinzhi {
	white,
	blue,
	purple,
	golden
};
enum zhuangtai {
	die,				//死亡状态		（死掉了）
	alive,				//还活着		（常备状态）
	hunshui,			//昏睡			(他睡着了！怎么办在线等！)
	daliwan,			//大力无穷		(服用了大力丸)		造成伤害时额外造成对方当前生命值10%加5点伤害
	xuruo,				//虚弱			(服用了虚弱剂)		造成伤害时降低伤害量的50%，同时X的闪避强制无效
	meihuo_acceptance,				//被魅惑		(中了魅惑技能)
	meihuo_attack,					//施加魅惑者
	//xinxinyan,			//心心眼		(好感度过高，意识都要混乱了)
	yinluan,			//淫乱			(服用了电脑配件)
	xuanyun,			//眩晕			无法发动技能，无法使用物品
	zhongdu,			//中毒			(服用了小毒丸)		每分钟降低5点生命值。
	bulaobusi			//不老不死		(吃了唐僧肉)		获得了长生不老、不死不灭之躯，无视所有伤害，但佛性慈悲，无法对他人造成伤害，直到每日清零
	//shihua,				//石化			(中了石化技能)		无法使用物品或技能，持续1分钟
	//fengshenjiahu			//风神加护		(不干正事的状态)

};
struct zhuangtai_acceptance {
	zhuangtai m_buff;
	xiling_player* m_attack;

	zhuangtai_acceptance() = delete;
	zhuangtai_acceptance(zhuangtai buff, xiling_player* other)
		:m_buff(buff), m_attack(other) {}
};
struct zhuangtai_attack {
	zhuangtai m_buff;
	xiling_player* m_acceptance;

	zhuangtai_attack() = delete;
	zhuangtai_attack(zhuangtai buff, xiling_player* other)
		:m_buff(buff), m_acceptance(other) {}
};

//struct jineng {
//	std::string name;
//	int bianhao;
//
//	virtual void use() = 0;
//	virtual void use(unsigned long long& _qq) = 0;
//};

class  xiling_player
{
	xiling_player()  = delete;				//不允许默认构造
public:

	std::string name;				//改名消耗法力
	unsigned long long qq;			//原生qq号码
	short id;						//player的id，从1开始, 0是叮当
	short health;					//生命值
	short healthmax;				//生命值上限
	short magic;					//法力值
	short magicmax;					//法力值上限
	int jinbi;						//金币数

	int liliang;				//力量，基础物理伤害
	int zhili;					//智力，基础法术伤害
	int wufang;					//物理防御力
	int fafang;					//法术防御力

	std::vector<std::unique_ptr<wupin>>beibao;			//背包
	std::vector<std::unique_ptr<jineng>>jinengbiao;		//技能表
	std::vector<zhuangtai>buff;			//状态表
	std::vector<zhuangtai_acceptance>buff_acceptance;
	std::vector<zhuangtai_attack>buff_attack;


	xiling_player(short IDMAX);
	xiling_player(short _id, unsigned long long _qq, std::string _name, short _health, short _magic, int _jinbi)
		:id(_id),qq(_qq),name(_name),health(_health),healthmax(100),magic(_magic),magicmax(100),jinbi(_jinbi),
		liliang(9),zhili(12),wufang(5),fafang(5){}
	xiling_player(short _id, unsigned long long _qq, std::string _name, short _health, short _magic, int _jinbi,
		int _liliang,int _zhili,int _wufang,int _fafang)
		:id(_id), qq(_qq), name(_name), health(_health), healthmax(100), magic(_magic), magicmax(100), jinbi(_jinbi),
		liliang(_liliang), zhili(_zhili), wufang(_wufang), fafang(_fafang){}
	xiling_player(const xiling_player& other)
		: name(other.name),qq(other.qq),id(other.id),health(other.health),healthmax(other.healthmax),magic(other.magic),
		magicmax(other.magicmax),jinbi(other.jinbi),liliang(other.liliang),zhili(other.zhili),wufang(other.wufang),
		fafang(other.fafang),buff(other.buff),buff_acceptance(other.buff_acceptance),buff_attack(other.buff_attack){
		if (!other.beibao.empty()) {
			for (int i = 0; i < other.beibao.size(); i++) {
				beibao.emplace_back(std::make_unique<wupin>(*other.beibao[i]));
			}
		}
			for (int i = 0; i < other.jinengbiao.size(); i++) {
				jinengbiao.emplace_back(std::make_unique<jineng>(*other.jinengbiao[i]));
			}
	}
	/// <summary>
	/// 默认构造函数的延续，紧跟，设置qq号
	/// </summary>
	/// <param name="_qq">qq号</param>
	void setqq(unsigned long long _qq) {
		qq = _qq;
	}

	/// <summary>
	/// 函数内已magic-10，请先行判断试图改名者的法力值>10,再调用这里
	/// </summary>
	/// <param name="_name">将要修改成什么名字</param>
	void setname(std::string _name) {
		magic -= 10;
		name = _name;
	}

	/// <summary>
	/// 展示player档案
	/// </summary>
	/// <returns>name+生命值法力值金币数</returns>
	std::string show();

	/// <summary>
	/// 展示背包
	/// </summary>
	/// <param name="showqq">将展示谁的背包（传入qq号）</param>
	/// <returns>只有其本人和我可以查询其背包</returns>
	std::string show_beibao(unsigned long long& showqq);

	/// <summary>
	/// 展示技能
	/// </summary>
	/// <param name="showqq">要展示谁的技能（传入qq号）</param>
	/// <returns>只有其本人和我可以查询技能详情，其他人可以看到技能名字</returns>
	std::string show_jineng(unsigned long long& showqq);

	int tili() {
		return healthmax / 10;
	}
	int jingshen() {
		return magicmax / 10;
	}
	void addtili(int tili) {
		health += tili * 10 * health / healthmax;
		healthmax += tili * 10;
	}
	void addjingshen(int jingshen) {
		magic += jingshen * 10 * magic / magicmax;
		magicmax += jingshen * 10;
	}


	void clear() {
		health = 100;
		healthmax = 100;
		magic = 110;
		magicmax = 100;
		jinbi = 100;
		beibao.clear();
		jinengbiao.clear();
		buff.clear();
	}

	
};

class  xiling_dangan {

	//不允许实例化
	xiling_dangan(){}

public:
	
	//从末尾加档案,返回添加成功的string到聊天区域,IDMAX 是档案人数
	static void addPlayer(std::vector<xiling_player>& _7fang,xiling_player sb, unsigned long long& _qq);

};


class  xiling_youxi
{
	xiling_youxi() {}//不允许实例化。

	 static laohuji_yuansu yao_suiji(int u);
	 static  std::string caizhadan(int caishu);
	 static std::string zengmeyao(const char yaofa);
	 static std::string show_laohuji(laohuji_yuansu yuansu[]);
	 static std::string to_screen(const laohuji_yuansu laohuji);
	 static bool laohuji_is_over();
	 static int laohuji_beilv(const int jiazhushu);
	 static  std::string yao_laohuji(std::vector<xiling_player>& _7fang,const int yaonage, const char yaofa, const char jiazhushu, const short player_id);
	
	

public:
	
	static void youxi_clear();

	/// <summary>
	/// 不许上传去掉游戏两个字的hear！
	/// </summary>
	static std::string send(std::vector<xiling_player>& _7fang,std::string& hear, short player_id);
};




namespace xiling_zhuangtai {

	/// <summary>
	/// 与伤害有关的都要过一遍遍！
	/// </summary>
	/// <param name="_7fang">档案</param>
	/// <param name="userid">自己id号</param>
	/// <param name="useto">对方id号</param>
	/// <param name="liang">受到/造成の伤害量</param>
	/// <param name="pangbai">游戏旁白，比如“XX受到大力丸加持，伤害增加X\n”</param>
	/// <returns>最终产生的伤害量，状态越界则返回 -1 </returns>
	int through_shanghai(std::vector<xiling_player>& _7fang, short userid,short useto,int liang,std::string& out_pangbai);


	//下面这个不对外
	/// <summary>
	/// 受到/造成伤害量在状态影响下
	/// </summary>
	/// <param name="_7fang">档案</param>
	/// <param name="one">这个状态</param>
	/// <param name="userid">使用者id号</param>
	/// <param name="useto">对方id号</param>
	/// <param name="liang">原本伤害量</param>
	/// <param name="pangbai">游戏旁白，比如“XX受到大力丸加持，伤害增加X\n”</param>
	/// <returns>最终伤害量</returns>
	inline int zhuangtai_shanghai(std::vector<xiling_player>& _7fang,short userid,short useto,int liang, std::string& out_pangbai);

}

