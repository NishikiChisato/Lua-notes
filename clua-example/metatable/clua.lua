-- create a point and try to change it
local p1 = point:pnew()
p1:pdis()
p1:pinc(1, 2)
p1:pdis()

-- create a line and try to change it
local l1 = line:lnew()
l1:ldis()
l1:linc(0, 1, 2)
l1:linc(1, 11, 12)
l1:ldis()

-- access its point
local sp1 = l1:lpoint(0)
local sp2 = l1:lpoint(1)

-- these two point has type 'point', which is the same with above
sp1:pdis()
sp2:pdis()

-- try to change these two point
sp1:pinc(1, 2)
sp2:pinc(11, 12)

-- it will change the line
l1:ldis()
