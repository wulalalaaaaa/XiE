#include"xiling_npc.h"
#include"xiling_jineng.h"
#include"xiling_wupin.h"


std::string xiling_npc::showall() {
	std::string a;
	a =  m_chenghao + m_name + 
		"\n生命值：" + tostring(m_health) + "/" + tostring(m_healthmax) +
		"\n法力值：" + tostring(m_magic) + "/" + tostring(m_magicmax) +
		"\n体力：" + tostring<int>(tili()) + " 精神：" + tostring<int>(jingshen()) +
		"\n力量：" + tostring<int>(m_liliang) + " 智力：" + tostring<int>(m_zhili) +
		"\n金币数：" + tostring(m_jinbi) +
		"\n技能：";
	for (int i = 0; i < jinengbiao.size(); i++) {
		a += jinengbiao[i]->m_name+" ";
	}

	a += "\n状态：";
	for (auto value : this->buff) {
		switch (value) {
		case die:a += "死亡，";
			break;
		case alive:a += "强调性存活中，";
			break;
		case hunshui:a += "昏睡，";
			break;
		case daliwan:a += "力大无穷，";
			break;
		case xuruo:a += "虚弱，";
			break;
		case meihuo_acceptance:a += "魅惑";
			break;
		case meihuo_attack:
			break;
		case yinluan:a += "yin乱，";
			break;
		case xuanyun:a += "眩晕，";
			break;
		case zhongdu:a += "中毒，";
			break;
		case bulaobusi:a += "不老不死，";
			break;
		default:a += "[未知状态]";
			break;
		}
	}

	return a;
}

std::string xiling_npc::showName()
{
	std::string a;
	a = m_chenghao + m_name + "\n" + m_show;

	return a;
}

std::string xiling_npc::showFalse()
{
	std::string a;
	a = m_chenghao + m_name +
		"\n生命值：???/???\n法力值：???/???\n体力：???  精神：???\n力量：???  智力：???\n金币数：???\n技能：???";
	a += "\n状态：";
	for (auto value : this->buff) {
		switch (value) {
		case die:a += "死亡，";
			break;
		case alive:a += "强调性存活中，";
			break;
		case hunshui:a += "昏睡，";
			break;
		case daliwan:a += "力大无穷，";
			break;
		case xuruo:a += "虚弱，";
			break;
		case meihuo_acceptance:a += "魅惑";
			break;
		case meihuo_attack:
			break;
		case yinluan:a += "yin乱，";
			break;
		case xuanyun:a += "眩晕，";
			break;
		case zhongdu:a += "中毒，";
			break;
		case bulaobusi:a += "不老不死，";
			break;
		default:a += "[未知状态]";
			break;
		}
	}
	a += "\n侦查失败，仅敌对关系时可强行侦查。";

	return a;
}
