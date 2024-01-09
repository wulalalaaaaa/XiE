
#include"xiling_jineng.h"
#include"xiling_player.h"
#include"xiling_wupin.h"
#include"basetools.h"
#include<vector>
#include<ctime>
#include<thread>

extern void zhuangtai_always(std::vector<xiling_player>& _7fang, short id, zhuangtai onebuff);
extern unsigned long long Member_huancun1;
extern unsigned long long Member_huancun2;
extern std::vector<xiling_player> _7fang;
extern int main_fadong_flag;

short find_id(unsigned long long& qqid);

void zhuangtai_60s(xiling_player* player, zhuangtai buff);
void zhuangtai_30s(xiling_player* player, zhuangtai buff);
inline void zhuangtai_acceptance_always(xiling_player& player, zhuangtai_acceptance&& buff);
inline void zhuangtai_attack_always(xiling_player& player, zhuangtai_attack&& buff);
void chixu_bingdun(int* fafang, int* wufang);

std::string(*jineng::find_jineng_fadong1(int bianhao))(std::vector<xiling_player>& _7fang, short userid){

	switch (bianhao) {

	case 2:return fadong_taixujianshen;


	case 10:return fadong_jubaoranhun;
	
	case 101:return fadong_putonggongji;
	case 102:return fadong_molichongji;

	case 105:return fadong_meihuo;
	case 106:return fadong_naonaonao;
	case 108:return fadong_bingdun;
	case 109:return fadong_zhongchui;
	case 110:return fadong_shanguangdan;
	case 111:return fadong_zhiliaoshu;
	case 115:return fadong_huoqiushu;
	case 117:return fadong_huanshounaqiu;
	case 118:return fadong_baonue;


	default:break;
	}
	return nullptr;
}
std::string(*jineng::find_jineng_fadong2(int bianhao))(std::vector<xiling_player>& _7fang, short userid, short useto){

	switch (bianhao) {

	case 2:return fadong_taixujianshen;


	case 10:return fadong_jubaoranhun;

	case 101:return fadong_putonggongji;
	case 102:return fadong_molichongji;

	case 105:return fadong_meihuo;
	case 106:return fadong_naonaonao;
	case 108:return fadong_bingdun;
	case 109:return fadong_zhongchui;
	case 110:return fadong_shanguangdan;
	case 111:return fadong_zhiliaoshu;
	case 115:return fadong_huoqiushu;
	case 117:return fadong_huanshounaqiu;
	case 118:return fadong_baonue;


	default:break;
	}
	return nullptr;
}

void jineng::incd(){
	m_incd = 1;
	std::thread getincd(chixu_cd,&m_incd,m_cdmax);
	getincd.detach();
	_log<errorlevel>(green, "此技能陷入冷却。").m_log();
}
void chixu_cd(bool* incd,int cdmax) {
	using namespace std::literals::chrono_literals;
	int cd10 = cdmax / 10;
	while (cd10--) {
		std::this_thread::sleep_for(10s);
	}
	(* incd) = 0;

	_log<errorlevel>(green, "技能冷却完毕。").m_log();
}










//专属技

//阿符 太虚剑神
std::string fadong_taixujianshen(std::vector<xiling_player>& _7fang, short userid) {
	return "发动失败，此技能必须指定目标发动。";
}
std::string fadong_taixujianshen(std::vector<xiling_player>& _7fang,short userid,short useto) {
	if (userid == 2) {
		if (_7fang[2].health < (int)(_7fang[2].healthmax * 0.6)) {
			return "生命值低于最大生命的60%，太虚剑神发动失败。";
		}
		else {
			std::string shuchu("阿符闭目凝神，沉寂如初。往昔种种，尽付此剑（法力值归零，生命值降低60%，\n难以言喻的能量散发，其中人抬眼凝意，\"神者，变化之极，妙万物而为言，不可以形诘者也。\"\n天云绽红滚动，一剑破之而出，直指");
			shuchu += _7fang[useto].name + " ——太虚剑神！\n 阿符造成0点伤害。";
			srand((unsigned)time(NULL));
			if ((rand() % 100) < 5) {
				_7fang[useto].buff.clear();
				_7fang[useto].buff.emplace_back(die);
				_7fang[useto].health = 0;
				shuchu += _7fang[useto].name + "死亡。";
			}
			return shuchu;
		}
	}
	else {
		return "阿符的专属技无法被他人发动";
	}
}

//千辉 聚爆燃魂
std::string fadong_jubaoranhun(std::vector<xiling_player>& _7fang, short userid) {
	return "发动失败，此技能必须指定目标发动。";
}
std::string fadong_jubaoranhun(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (userid == 10) {
		if (_7fang[10].magic > 30) {
			_7fang[10].magic -= 30;
			std::thread qianhui(chixu_qianhuizhuanshu,&_7fang);
			qianhui.detach();
			_7fang[useto].buff.push_back(xuanyun);
			int liang = (_7fang[10].liliang - _7fang[useto].wufang / 2) * 3 / 2;
			//再through一遍状态，最后赋值到生命
			std::string shuchu("");
			liang = xiling_zhuangtai::through_shanghai(_7fang, userid, useto, liang, shuchu);
			shuchu += "千辉共造成" + tostring<int>(liang) + " 点伤害。";
			return shuchu;
		}
		else {
			return "法力值不足，聚爆燃魂发动失败";
		}

	}
	else {
		return "千辉的专属技无法被他人发动";
	}

}
void chixu_qianhuizhuanshu(std::vector<xiling_player>* _7fang) {
	using namespace std::literals::chrono_literals;
	(* _7fang)[10].liliang += 10;
	_log<errorlevel>(green, "千辉力量+10增益状态开始，将持续60秒。").m_log();

	std::this_thread::sleep_for(60s);
	
	(*_7fang)[10].liliang -= 20;
	_log<errorlevel>(green, "千辉力量+10增益结束，陷入60秒虚脱状态。").m_log();

	std::this_thread::sleep_for(60s);

	(*_7fang)[10].liliang += 10;
	_log<errorlevel>(green, "千辉虚脱状态结束，回归正常数值。").m_log();
}



//通用技

//普通攻击
std::string fadong_putonggongji(std::vector<xiling_player>& _7fang, short userid){
	return "发动失败，此技能必须指定目标发动。";
}
std::string fadong_putonggongji(std::vector<xiling_player>& _7fang, short userid, short useto){
	int shanghai = _7fang[userid].liliang - _7fang[useto].wufang;
	std::string shuchu("成功发动普通攻击，");
	shanghai < 0 ? shanghai = 0 : shanghai = xiling_zhuangtai::through_shanghai(_7fang,userid,useto,shanghai,shuchu);
	shuchu += _7fang[userid].name + "共造成" + tostring<int>(shanghai) + "点伤害。";
	_7fang[useto].health -= shanghai;
	return shuchu;
}
//魔力冲击
std::string fadong_molichongji(std::vector<xiling_player>& _7fang, short userid){
	return "发动失败，此技能必须指定目标发动。";
}
std::string fadong_molichongji(std::vector<xiling_player>& _7fang, short userid, short useto){
	if (_7fang[userid].magic > 2) { _7fang[userid].magic -= 3; }
	else { return "法力不足，发动失败。"; }
	int shanghai = _7fang[userid].zhili - _7fang[useto].fafang;
	std::string shuchu("成功发动魔力冲击，");
	shanghai < 0 ? shanghai = 0 : shanghai = xiling_zhuangtai::through_shanghai(_7fang, userid, useto, shanghai, shuchu);
	_7fang[useto].health -= shanghai;
	return shuchu;
}

//魅惑
std::string fadong_meihuo(std::vector<xiling_player>& _7fang, short userid) {
	if (_7fang[userid].magic > 29) { _7fang[userid].magic -= 30; }
	else { return "法力不足，发动失败。"; }
	zhuangtai_30s(&_7fang[userid], xuanyun);
	return _7fang[userid].name+"对自己发动了魅惑，美呆了！"+ _7fang[userid].name + "对着镜子陷入眩晕状态持续30s。";
}
std::string fadong_meihuo(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (_7fang[userid].magic > 29) { _7fang[userid].magic -= 30; }
	else { return "法力不足，发动失败。"; }
	zhuangtai_always(_7fang, useto, meihuo_acceptance);
	zhuangtai_always(_7fang, userid, meihuo_attack);
	zhuangtai_acceptance_always(_7fang[useto], zhuangtai_acceptance(meihuo_acceptance,&_7fang[userid]));
	zhuangtai_attack_always(_7fang[userid], zhuangtai_attack(meihuo_attack, &_7fang[useto]));
	return _7fang[userid].name+"对"+_7fang[useto].name+"发动了魅惑，"+_7fang[useto].name + "感觉自己有些奇怪并时不时看向"+ _7fang[userid].name;
}
//挠挠挠
std::string fadong_naonaonao(std::vector<xiling_player>& _7fang, short userid) {
	return "发动失败，此技能必须指定目标发动。";
}
std::string fadong_naonaonao(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (_7fang[userid].magic > 9) { _7fang[userid].magic -= 10; }
	else { return "法力不足，发动失败。"; }
	std::string shuchu("");
	int shanghai = 1;
	shanghai = xiling_zhuangtai::through_shanghai(_7fang, userid, useto, shanghai, shuchu);
	if (shanghai == 0) 
		return shuchu; 
	shuchu = "";
	if (_7fang[userid].liliang < 8) {
		if (_7fang[userid].liliang < 4) {
			if (_7fang[userid].liliang < 2) {
				//shanghaicishu = 70;
				_7fang[useto].health -= 70;
				shuchu = _7fang[userid].name + "的爪子挥出了残影！-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1";
				
			}
			else {
				//shanghaicishu = 60;
				_7fang[useto].health -= 60;
				shuchu = _7fang[userid].name + "的爪子挥出了残影！-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1";
			}
		}
		else {
			if (_7fang[userid].liliang < 6) {
				//shanghaicishu = 50;
				_7fang[useto].health -= 50;
				shuchu = _7fang[userid].name + "的爪子挥出了残影！-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1";
			}
			else {
				//shanghaicishu = 40;
				_7fang[useto].health -= 40;
				shuchu = _7fang[userid].name + "的爪子挥出了残影！-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1-1";
			}
		}
	}
	else {
		if (_7fang[userid].liliang < 12) {
			if (_7fang[userid].liliang < 10) {
				//shanghaicishu = 10;
				_7fang[useto].health -= 10;
				shuchu = _7fang[userid].name + "亮出了爪子！-1-1-1-1-1-1-1-1-1-1";
			}
			else {
				//shanghaicishu = 5;
				_7fang[useto].health -= 5;
				shuchu = _7fang[userid].name + "亮出了爪子！-1-1-1-1-1";
			}
		}
		else {
			//shanghaicishu = 2;
			_7fang[useto].health -= 2;
			shuchu = _7fang[userid].name + "的爪子挥不动！-1-1";
		}
	}

	if (_7fang[useto].health < 1) {
		zhuangtai_always(_7fang, useto, die);
		shuchu += "\n" + _7fang[useto].name + "死亡。";
	}
	return shuchu;
}
//冰盾
std::string fadong_bingdun(std::vector<xiling_player>& _7fang, short userid) {
	if (_7fang[userid].magic > 9) { _7fang[userid].magic -= 10; }
	else { return "法力不足，发动失败。"; }
	std::thread bingdun(chixu_bingdun, &_7fang[userid].fafang,&_7fang[userid].wufang);
	bingdun.detach();

	return "冰盾萦绕，提高了5点物理防御和5点法术防御，持续1min。";
}
std::string fadong_bingdun(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (_7fang[userid].magic > 9) { _7fang[userid].magic -= 10; }
	else { return "法力不足，发动失败。"; }
	std::thread bingdun(chixu_bingdun, &_7fang[useto].fafang, &_7fang[useto].wufang);
	bingdun.detach();
	return "冰盾萦绕，成功为 "+_7fang[useto].name + " 提高了5点物理防御和5点法术防御，持续1min。";
}
//重锤
std::string fadong_zhongchui(std::vector<xiling_player>& _7fang, short userid) {
	return "发动失败，此技能必须指定目标发动。";
}
std::string fadong_zhongchui(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (_7fang[userid].magic > 14) { _7fang[userid].magic -= 15; }
	else { return "法力不足，发动失败。"; }
	std::string shuchu("一轮重锤从天而降！");
	int shanghai = _7fang[userid].liliang + 10  - _7fang[useto].wufang;
	shanghai < 0 ? shanghai = 0 : shanghai = xiling_zhuangtai::through_shanghai(_7fang, userid, useto, shanghai, shuchu);

	short id1 = find_id(Member_huancun1);
	short id2 = find_id(Member_huancun2);
	_7fang[useto].health -= shanghai;
	_7fang[id1].health -= static_cast<int>(shanghai * 0.8);
	_7fang[id2].health -= static_cast<int>(shanghai * 0.5);

	shuchu += "共造成" + tostring<int>(shanghai) + "点伤害，同时重锤的震荡造成"+_7fang[id1].name+ "被震伤" +
		tostring<int>(static_cast<int>(shanghai * 0.8))+"点伤害，"+_7fang[id2].name+"被震伤"+
		tostring<int>(static_cast<int>(shanghai * 0.5))+"点伤害。";

	if (_7fang[useto].health < 1) {
		zhuangtai_always(_7fang, useto, die);
		shuchu += "\n" + _7fang[useto].name + "死亡。";
	}
	if (_7fang[id1].health < 1) {
		zhuangtai_always(_7fang, id1, die);
		shuchu += "\n" + _7fang[id1].name + "死亡。";
	}
	if (_7fang[id2].health < 1) {
		zhuangtai_always(_7fang, id2, die);
		shuchu += "\n" + _7fang[id2].name + "死亡。";
	}
	return shuchu;
}
//闪光弹
std::string fadong_shanguangdan(std::vector<xiling_player>& _7fang, short userid) {
	if (_7fang[userid].magic > 4) { _7fang[userid].magic -= 5; }
	else { return "法力不足，发动失败。"; }
	main_fadong_flag = 1;
	return "结合声波魔法和光魔法，一枚闪光弹激震而出。伴着眩晕的耳鸣，全场所有人都瞎了！";
}
std::string fadong_shanguangdan(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "发动失败，此技能无法指定目标发动。";
}
//治疗术
std::string fadong_zhiliaoshu(std::vector<xiling_player>& _7fang, short userid) {
	if (_7fang[userid].magic > 9) { _7fang[userid].magic -= 10; }
	else { return "法力不足，发动失败。"; }
	_7fang[userid].health += 20;
	if (_7fang[userid].health > _7fang[userid].healthmax)_7fang[userid].health = _7fang[userid].healthmax;
	return _7fang[userid].name+"双手合十默默吟唱，温和的光点缓缓浮出，生命值恢复20点。";
}
std::string fadong_zhiliaoshu(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (_7fang[userid].magic > 9) { _7fang[userid].magic -= 10; }
	else { return "法力不足，发动失败。"; }
	_7fang[useto].health += 20;
	if (_7fang[useto].health > _7fang[useto].healthmax)_7fang[useto].health = _7fang[useto].healthmax;
	return _7fang[userid].name + "双手合十默默吟唱，温和的光点缓缓浮出，为"+_7fang[useto].name + "恢复20点生命值。";
}
//火球术
std::string fadong_huoqiushu(std::vector<xiling_player>& _7fang, short userid) {
	return "发动失败，此技能必须指定目标发动。";
}
std::string fadong_huoqiushu(std::vector<xiling_player>& _7fang, short userid, short useto) {
	if (_7fang[userid].magic > 29) { _7fang[userid].magic -= 30; }
	else { return "法力不足，发动失败。"; }
	int shanghai = _7fang[userid].zhili + 15 - static_cast<int>(_7fang[useto].fafang * 0.5);
	std::string shuchu("随着法阵成型，魔力产生微妙的共鸣，骤然暴动为高温火球激出\n");
	shanghai < 0 ? shanghai = 0 : shanghai = xiling_zhuangtai::through_shanghai(_7fang, userid, useto, shanghai, shuchu);
	_7fang[useto].health -= shanghai;
	shuchu += "共造成"+tostring<int>(shanghai)+"点伤害。";
	if (_7fang[useto].health < 1) {
		zhuangtai_always(_7fang, useto, die);
		shuchu += "\n" + _7fang[useto].name + "死亡。";
	}
	return shuchu;
}
//换手拿球
std::string fadong_huanshounaqiu(std::vector<xiling_player>& _7fang, short userid) {
	static bool huanshounaqiu_qiu = 0;
	if (huanshounaqiu_qiu) {
		return "成功发动换手拿球，你把球从左手换到了右手。";
	}
	else {
		return "成功发动换手拿球，你把球从右手换到了左手。";
	}
}
std::string fadong_huanshounaqiu(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "发动失败，此技能无法指定目标发动。";
}
//暴虐
std::string fadong_baonue(std::vector<xiling_player>& _7fang, short userid) {
	if (_7fang[userid].magic > 39) {
		std::thread baonue(chixu_baonue, &_7fang[userid]);
		baonue.detach();
		return "猩红的血阵引起汹涌的漩涡，"+_7fang[userid].name + "血脉暴起，根骨临时大幅躁动！力量+15，体力-2，精神-5，将于30s后陷入20s虚弱状态。";
	}
	else { return "法力不足，发动失败。"; }
}
std::string fadong_baonue(std::vector<xiling_player>& _7fang, short userid, short useto) {
	return "目前无法对他人的血液产生躁动。";
}









//暴虐 的持续效果
void chixu_baonue(xiling_player* player) {
	using namespace std::literals::chrono_literals;

	player->magic -= 40;
	player->healthmax -= 20;
	if (player->health > player->healthmax)player->health = player->healthmax;
	player->magicmax -= 50;
	if (player->magic > player->magicmax)player->magic = player->magicmax;
	player->liliang += 15;

	std::this_thread::sleep_for(30s);

	player->healthmax += 20;
	player->liliang -= 20;

	std::this_thread::sleep_for(20s);

	player->magicmax += 50;
	player->liliang += 5;

}
//冰盾 的持续效果
void chixu_bingdun(int* fafang, int* wufang) {
	using namespace std::literals::chrono_literals;

	(* fafang) += 5;
	(* wufang) += 5;

	std::this_thread::sleep_for(60s);

	(*fafang) -= 5;
	(*wufang) -= 5;
}


void zhuangtai_60s(xiling_player* player, zhuangtai buff) {
	using namespace std::literals::chrono_literals;

	player->buff.push_back(buff);
	int i = player->buff.size() - 1;
	_log<errorlevel>(green, player->name + " 获得" + tostring((int)buff) + "号状态，将持续10分钟 。 ").m_log();


	std::this_thread::sleep_for(60s);


	player->buff.erase(player->buff.begin() + i);
	_log<errorlevel>(green, player->name + " 的 " + tostring((int)buff) + "号状态 持续时间结束 。 ").m_log();

}

void zhuangtai_30s(xiling_player* player, zhuangtai buff) {
	using namespace std::literals::chrono_literals;

	player->buff.push_back(buff);
	int i = player->buff.size() - 1;
	_log<errorlevel>(green, player->name + " 获得" + tostring((int)buff) + "号状态，将持续10分钟 。 ").m_log();


	std::this_thread::sleep_for(30s);


	player->buff.erase(player->buff.begin() + i);
	_log<errorlevel>(green, player->name + " 的 " + tostring((int)buff) + "号状态 持续时间结束 。 ").m_log();

}

inline void zhuangtai_acceptance_always(xiling_player& player, zhuangtai_acceptance&& buff) {
	player.buff_acceptance.emplace_back(buff);
}
inline void zhuangtai_attack_always(xiling_player& player, zhuangtai_attack&& buff) {
	player.buff_attack.emplace_back(buff);
}

short find_id(unsigned long long& qqid) {
	for (short i = 0; i < _7fang.size(); i++) {
		if (_7fang[i].qq == qqid) {
			return i;
		}
	}
}