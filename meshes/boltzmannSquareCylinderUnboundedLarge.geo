cl__1 = 1;
r1 = 0.1/6;
r2 = 0.05/6;
Point(1) = {-4, -4, 0, r1};
Point(2) = {8, -4, 0, r1};
Point(3) = {8, 4, 0, r1};
Point(4) = {-4, 4, 0, r1};
Point(5) = {-6, -6, 0, r1};
Point(6) = {-4, -6, 0, r1};
Point(7) = {8, -6, 0, r1};
Point(8) = {10, -6, 0, r1};
Point(9) = {10, -4, 0, r1};
Point(10) = {10, 4, 0, r1};
Point(11) = {10, 6, 0, r1};
Point(12) = {8, 6, 0, r1};
Point(13) = {-4, 6, 0, r1};
Point(14) = {-6, 6, 0, r1};
Point(15) = {-6, 4, 0, r1};
Point(16) = {-6, -4, 0, r1};
Point(17) = {-0.25, -0.3, 0, r2};
Point(18) = {0.25, -0.3, 0, r2};
Point(19) = {0.25, 0.2, 0, r2};
Point(20) = {-0.25, 0.2, 0, r2};
Line(1) = {5, 6};
Line(2) = {6, 1};
Line(3) = {1, 16};
Line(4) = {16, 5};
Line(5) = {6, 7};
Line(6) = {7, 2};
Line(7) = {2, 1};
Line(8) = {7, 8};
Line(9) = {8, 9};
Line(10) = {9, 2};
Line(11) = {9, 10};
Line(12) = {10, 3};
Line(13) = {3, 2};
Line(15) = {12, 3};
Line(16) = {10, 11};
Line(17) = {11, 12};
Line(18) = {12, 13};
Line(19) = {13, 4};
Line(20) = {4, 3};
Line(21) = {13, 14};
Line(22) = {14, 15};
Line(23) = {15, 4};
Line(24) = {4, 1};
Line(25) = {16, 15};
Line(26) = {17, 18};
Line(27) = {18, 19};
Line(28) = {19, 20};
Line(29) = {20, 17};
Line Loop(31) = {25, 23, 24, 3};
Plane Surface(31) = {31};
Line Loop(33) = {22, 23, -19, 21};
Plane Surface(33) = {33};
Line Loop(36) = {20, 13, 7, -24, -29, -28, -27, -26};
Plane Surface(36) = {36};
Line Loop(38) = {4, 1, 2, 3};
Plane Surface(38) = {38};
Line Loop(40) = {6, -10, -9, -8};
Plane Surface(40) = {40};
Line Loop(42) = {16, 17, 15, -12};
Plane Surface(42) = {42};
Line Loop(44) = {11, 12, 13, -10};
Plane Surface(44) = {44};
Line Loop(46) = {18, 19, 20, -15};
Plane Surface(46) = {46};
Line Loop(48) = {7, -2, 5, 6};
Plane Surface(48) = {48};
Physical Line("Wall",1) = {26, 27, 28, 29};
Physical Line("Slip") = {1, 5, 8, 17, 18, 21};
Physical Line("Inflow") = {4, 22, 25};
Physical Line("Outflow") = {9, 11, 16};
Physical Surface("Domain") = {31, 33, 36, 38, 40, 42, 44, 46, 48};