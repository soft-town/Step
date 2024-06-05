// Step.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <iostream>
#include <string>
#include <list>
#include <vector>  
#include <algorithm>
#include <direct.h>

using namespace std;
struct _pair
{
	float t;
	float v;
	_pair() {
		t = v = 0;
	}
	_pair(float t_, float v_) {
		t = t_, v = v_;
	}
};

//
bool pair_compare_x(const _pair& p1, const _pair& p2)
{
	return p1.t < p2.t;
}

//
bool read_csv(char *fname, vector<_pair> &g)
{
	FILE* fp = fopen(fname, "r");
	if (fp == NULL)
		return false;

	char buff[256];
	fgets(buff, 256, fp); // ヘッダ
	fgets(buff, 256, fp); // ヘッダ2

	float t, v;
	while (fgets(buff, 256, fp)) {
		if (buff[0] == '\n') 
			continue;
		2 == sscanf(buff, "%f %f\n", &t, &v);
		g.push_back(_pair(t, v));
	}
	fclose(fp);

	sort(g.begin(), g.end(), pair_compare_x);
	return true;
}

//
bool write_csv(char* fname, vector<_pair>& g)
{
	FILE* fp = fopen(fname, "w");
	if (fp == NULL)
		return false;

	char buff[256];
	fprintf(fp, "Time[s]	Normalized voltage[p.u.]\n");
	fprintf(fp, "dummy\n");

	for (int i=0; i<g.size(); i++) {
		fprintf(fp, "%10.4e %10.4e\n", g[i].t, g[i].v);
	}
	fclose(fp);

	return true;
}

// 関数(g)が単調増加を開始する時刻orgを求めるはずだが、
// g[i].tが0になる点を求めるようにした
//
float detOrg(vector<_pair>& g)
{
	float v = g[0].v;
	int i = 0;
	while (g[i].v<=v) {
		v = g[i++].v;
	}
	while (g[i].t <= 0) {
		i++;
	}
	printf("O1=%d, %10.3e, %10.3e\n", i, g[i].t, v);
	return g[i].t;	
}

// 関数(1-g)をfromからtoまで積分

float integralT(float from, float to, vector<_pair>& g)
{
	printf("integralT [%10.3e,%10.3e]\n", from, to);
	int i;
	for (i = 0; g[i].t < from; i++);
	float s = 0;
	for (i++; g[i].t < to; i++) {
		s += (1 - g[i].v) * (g[i].t - g[i-1].t);
	}
	return s;
}

// 関数(1-g)をfromから全区間積分
 
void integralT(float from, vector<_pair>& g, vector<_pair>& G)
{
	int i;
	for (i = 0; g[i].t < from; i++) {
		G.push_back(_pair(g[i].t, 0));
	}
	float s = 0;
	for (; i<g.size(); i++) {
		if (g[i].t < 0) 
			break;
		s += (1 - g[i].v) * (g[i].t - g[i - 1].t);
		G.push_back(_pair(g[i].t, s));
	}
}


// g(t)=1となる最小のtに対するintegralT(0,t)

float calc_Ta(vector<_pair>& g)
{
	float eps = 1e-3;
	int i;
	for (i = 1; i < g.size(); i++) {
		if (fabs(g[i].v - 1) < eps) break;
	}
	if (i < g.size()) {
		return integralT(g[0].t, g[i].t, g);
	}
	return 0;
}

// fabs(TN-G(t))<0.02tが成り立つ最小のt

float search_ts(vector<_pair>& G, float TN)
{
	int i;
	for (i = G.size()-1; 0 <= i; i--) {
		if(0.02 * G[i].t < fabs(G[i].v - TN)) break;
	}
	if (0<=i) {
		return G[i].t;
	}
	return 0;
}

// Main
int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: step input-file output-dir\n");
	}

	if (_mkdir(argv[2])) {
		printf("%sは既存です\n", argv[2]);
	}

	// 入力ファイル読み込み
	char buff[256];
	sprintf(buff, "%s\\%s", argv[2], argv[1]);
	vector<_pair> g;
	if (!read_csv(buff, g))
		return -1;

	// 時間ソートしたデータを保存
	sprintf(buff, "%s\\S-%s", argv[2], argv[1]);
	write_csv(buff, g);

	// TN実験応答時間 T(2*tmax)
	float tmin = 0.42e-6*2;
	float tmax = 3.12e-6/2;
	float TN = 0;   // 実験応答時間
	float Ta = 0;   // 部分応答時間
	float ts = 0;   // 整定時間
	float beta = 0; // オーバーシュート
	float O1 = detOrg(g);
	TN = integralT(O1, 2*tmax, g);
	printf("TN=%10.3e\n", TN);

	// Ta 部分応答時間 max(T(t)) t<2*tmax, 通常Ta=T(t1)ここでt1はg(t)=1となる最小のt
	Ta = calc_Ta(g);
	printf("Ta=%10.3e\n", Ta);

	// ts 整定時間
	vector<_pair> G;
	integralT(O1, g, G);
	sprintf(buff, "%s\\S-T(t).txt", argv[2]);
	write_csv(buff, G);

	ts = search_ts(G, TN);
	printf("ts=%10.3e\n", ts);

	return 0;
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//   1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
