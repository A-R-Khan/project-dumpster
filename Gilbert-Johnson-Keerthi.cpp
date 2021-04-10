#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;

#ifndef SIGNUM(x)
#define SIGNUM(x) (x == 0 ? 0 : (x < 0 ? -1 : 1))
#endif

#define IN_EPS(x) (x>=-1e-5 && x<=1e-5)

#ifndef vect
#define vect point
#endif

// struct to represent 2d points or vectors with the following operations defined
// 1. arithmetic (add, subtract, scale)
// 2. dot product
// 3. cross product
// 4. norm
// 5. vector triple product of the form (A X B) X C
struct point {

private:
	double _norm = 0;
	double _inv_norm = 0;

public:
	double x;
	double y;

	point(double x, double y){this->x = x; this->y = y;}

	point(point &p){
		this->x = p.x;
		this->y = p.y;
	}

	double inv_norm(){
		if(_inv_norm != -1){
			return _inv_norm;
		}
		return 1.0/this->norm();
	}

	double norm(){
		if(_norm!=-1){
			return _norm;
		}
		this->_norm = sqrt(this->x*this->x + this->y*this->y);
		return _norm;
	}

	// the (ingenious) algorithm from Quake 3
	// no reason to use it instead of 1.0/std::sqrt() 
	// it just looks beautiful so try it out
	double f_inv_norm(){
		double number = this->norm();
	    union {
	        float f;
	        uint32_t i;
	    } conv;

	    float x2;
	    const float threehalfs = 1.5F;

	    x2 = number * 0.5F;
	    conv.f  = number;
	    conv.i  = 0x5f3759df - ( conv.i >> 1 );
	    conv.f  = conv.f * ( threehalfs - ( x2 * conv.f * conv.f ) );
	    return conv.f;
	}

	point &scale(double factor){
		this->x *= factor;
		this->y *= factor;

		return *this;
	}

	point &normalize(){
		return this->scale(this->inv_norm());
	}

	// 2d cross product
	double cross(point &p2){
		return this->x*p2.y - this->y*p2.x;
	}

	// dot product
	double dot(point p1, point &p2){
		return p1.x*p2.x + p1.y*p2.y;
	}

	// (this X p2) X p3
	point& triple_cross(point &p2, point &p3){
		double a = this->cross(p2);
		this->x = -a*p3.x;
		this->y = a*p3.y;
		return *this;
	}

	//Addition
	point operator+=(point pnt){ (*this).x += pnt.x; (*this).y += pnt.y; return (*this); }
	point operator+(point pnt) { return point((*this).x + pnt.x, (*this).y + pnt.y);	}

	//Subtraction
	point operator-=(point pnt){ (*this).x -= pnt.x; (*this).y -= pnt.y; return (*this); }
	point operator-(point pnt) { return point((*this).x - pnt.x, (*this).y - pnt.y); }

	//Multiplication
	point operator*=(float num){ (*this).x *= num; (*this).y *= num; return (*this); }
	point operator*(float num) { return point((*this).x * num, (*this).y * num); }

	//Division
	point operator/=(float num){ (*this).x /= num; (*this).y /= num; return (*this); }
	point operator/(float num) { return point((*this).x / num, (*this).y / num); }

	//Equal (Assignment)
	point operator=(point pnt) { (*this).x = pnt.x; (*this).y = pnt.y; return (*this); }
};

// 2d triple product ((A X B) X C)
// for the other association use unary minus after the operation
point triple_crossed(point &p1, point &p2, point &p3){
	double a = p1.cross(p2);
	return point(-a*p3.x, a*p3.y);
}


point normalized(point &p){
	return point(p.x/p.norm(), p.y/p.norm());
}

point scaled(point &p, double factor){
	return point(p.x*factor, p.y*factor);
}

point origin() {
	return point(0.0, 0.0);
}

int compare_angle(point &p11, point &p12, point &p21, point &p22){
	return SIGNUM((p12-p11)*(p22-p21));
}

// modular increment and decrement for circular array operations

inline int modinc(int b, int n){
	return (b + 1 == n ? 0: b + 1);
}
inline int moddec(int b, int n){
	return (b-1 == -1 ? n-1 : b-1);
}


// struct to represent 2-simplexes (better known as triangles)
// why can't mathematicians just talk normally

struct simplex {

	point p1;
	point p2;
	point p3;

	simplex(point p1, point p2, point p3){this->p1 = p1; this->p2 = p2; this->p3 = p3;}

	// source1 : http://totologic.blogspot.com/2014/01/accurate-point-in-triangle-test.html
	// try using the winding number algorithm (maybe overkill?)
	// nvm, winding number is same as dot product
	// use epsilon-bounded checks later

	// MEMO : done rudimentary epsilon cheks and bounding box checks

	bool contains_origin(){

		// just some bounding box checks
		if(!(min(p1.x, min(p2.x, p3.x)) <= 0 && max(p1.x, max(p2.x, p3.x)) >=0 && min(p1.y, min(p2.y, p3.y)) >= 0 && max(p1.y, max(p2.y, p3.y)) <= 0)) return false;

		double e1 = (p2-p1).dot(p1);
		double e2 = (p3-p2).dot(p2);
		double e3 = (p1-p3).dot(p3);

		// TODO not sure about the sign here so try running some tests
		// epsilon bounds should be tuned
		if((e1 <= 0 || IN_EPS(e1)) && (e2 <= 0 || IN_EPS(e2)) && (e3 <= 0 || IN_EPS(e3))){
			return true;
		}
		return false;

	}

};


// currently under work so O(n/2)
// should be able to acheive O(log(n)) for convex polygons
// check some resources
// can get log complexity using the fact that dot is weakly decreasing,
// has only one optimum and so can be considered to form two opposite ended sorted arrays

// MEMO searched stackoverflow and researchgate, only found O(n) implementations
// search more
// test out log(n) algorithm later (on python?)

int support_point_fast(vector<point> &polygon, int pos){

	// initial guess is the point at half the index (circular)
	int sp = (pos + (polygon.size()/2)) % polygon.size();

	// satisfied when both steps cause a decrease
	// go in the direction of ascent otherwise, until satisfied
	double pi_dot, pd_dot;
	int sz = polygon.size();
	bool satisfied = false;
	double sp_dot;

	point norm_ref = -normalized(polygon[pos]);

	double max_dot = 1;

	while(!satisfied){
		sp_dot = polygon[sp].dot(norm_ref);
		pi_dot = polygon[modinc(sp, sz)].dot(norm_ref);
		if(pi_dot == max_dot){
			pi_dot = 0;
		}
		pd_dot = polygon[moddec(sp, sz)].dot(norm_ref);
		if(pd_dot == max_dot){
			pd_dot = 0;
		}

		if(sp_dot >= pi_dot && sp_dot >= pd_dot){
			break;
		}
		else if(pi_dot > pd_dot){
			sp = modinc(sp, sz);
		}
		else if(pd_dot > pi_dot){
			sp = moddec(sp, sz);
		}
	}
	return sp;
}


// naive algorithm O(n)
// change this to log(n) asap
// MEMO keep the log(n) idea on hold maybe O(n) is optimal

int support_point(vector<point> &polygon, int pos){
	int max_dot = 0;
	int sp = 0;
	double dot = 0;
	point norm_ref = -normalized(polygon[pos]);
	for(int i = 0; i<polygon.size(); ++i){
		if(i == pos){
			continue;
		}
		dot = normalized(polygon[i]).dot(norm_ref);
		if(dot > max_dot){
			sp = i;
			max_dot = dot;
		}
	}
	return sp;
}

int support_point(vector<point> &polygon, point pt){

	int sp = 0;
	double dot = 0;
	point norm_ref = normalized(pt);
	double max_dot = 0;

	for(int i = 0; i<polygon.size(); ++i){
		dot = normalized(polygon[i]).dot(norm_ref);
		if(dot > max_dot){
			sp = i;
			max_dot = dot;
		}
	}
	return sp;
}

vector<point> minkowski_sum(vector<point> &a, vector<point> &b) {

	//	ROTATE TO LOWEST (x, y)
	//	FOREACH X, Y
	//		R<=X+Y
	//		poX == poY X++ Y++ continue
	//		poX > poY ? Y++ : X++

	int posa = 0;
	int mnx = a[0].x, mny = a[0].y;
	for(int i = 0; i<a.size(); ++i) {
		if(a[i] > mny){
			continue;
		}
		else if(a[i].y < mny) {
			posa = i;
			mny = a[i].y;
			mnx = a[i].x;
		}
		else if(a[i].y == mny && a[i].x < mnx){
			posa = i;
			mny = a[i].y;
			mnx = a[i].x;
		}
	}

	int posb = 0;
	mnx = b[0].x;
	mny = b[0].y;
	for(int i = 0; i<b.size(); ++i) {
		if(b[i].y > mny){
			continue;
		}
		else if(b[i].y < mny) {
			posb = i;
			mny = b[i].y;
			mnx = b[i].x;
		}
		else if(b[i].y == mny && b[i].x < mnx){
			posb = i;
			mny = b[i].y;
			mnx = b[i].x;
		}
	}

	int asz = a.size();
	int bsz = b.size();

	vector<point> mnk_sum(asz + bsz);

	int i = posa, j = posb;
	int cnt = 0;
	int cmp;

	// merging routine
	while(cnt < asz + bsz){

		mnk_sum[cnt] = a[i] + b[j];
		cmp = compare_angle(a[modinc(i, asz)] - a[i], b[modinc(j, bsz)] - b[j]);
		if(cmp == 1){
			i = modinc(i, asz);
		}
		else if(cmp == -1){
			j = modinc(j, bsz);
		}
		else {
			i = modinc(i, asz);
			j = modinc(j, asz);
		}
		cnt++;
	}

	return mnk_sum;
}

vector<point> minkowski_difference(vector<point> &a, vector<point> &b) {

	// just sum with all points in b negated
	vector<point> minus_b = vector<point>(b.size());
	int i = 0;
	while(i < b.size()){
		minus_b[i] = -b[i];
		++i;
	}
	return minkowski_sum(a, minus_b);
}


bool intersects(vector<point> &pg1, vector<point> &pg2){

	vector<point> mnk_diff = minkowski_difference(pg1, pg2);

	// do not instantiate origin for no reason
	// just use unary minus or scale by -1
	// if absolutely necessary, use this :
	point O = origin();
	// and stop crying about python being better

	// any random point
	int p = 0;
	int sp = support_point(mnk_diff, 0);
	int pn;

	while (true) {
		point line = polygon[sp] - polygon[p];
		point perp = normalized(triple_crossed(line, -polygon[p], line));
		pn = support_point(mnk_diff, perp);

		// support point remains same so we have reached end
		if(pn == sp || pn == p) {
			return false;
		}
		else if(simplex(mnk_diff[p], mnk_diff[sp], mnk_diff[pn]).contains_origin()){
			return true;
		}
		sp = pn;
	}
}

int main() {
	int n1, n2;
	cout << "Enter number of sides in polygons separated by a [space]" << endl;
	cin >> n1 >> n2;

	vector<point> p1(n1);
	vector<point> p2(n2);

	int i = 0;
	cout << "Enter points in first polygon in the format x [space] y on the next " << n1 << " lines" << endl;
	while(i<n1) {
		cin >> p1[i].x >> p1[i].y;
		--i;
	}

	cout << "Enter points in second polygon in the format x [space] y on the next " << n2 << " lines" << endl;
	while(i<n2) {
		cin >> p2[i].x >> p2[i].y;
		--i;
	}

	cout << endl << (intersects(p1, p2) ? "INTERSECTION FOUND" : "NO INTERSECTION") << endl;

	return 0;
}