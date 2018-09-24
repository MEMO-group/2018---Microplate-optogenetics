// model resolution
$fs=0.5;
$fa=5;

// ANSI standard microplate (dimensions in mm)
base_l = 127.76;
base_w = 85.48;
corner_r = 3.18;
flange_h = 2.41;
flange_w = 1.27;
plate_h = 14.35;
chamfer = 7; // optional, size not specified
interruption_x0 = 47.8; // distance between edges of interruption and edges of plate
interruption_h = 6.85;

n_rows = 8;
n_cols = 12;

well_x0 = 14.38; // distance between the left outside edge of the plate and the center of the first column of wells
well_y0 = 11.24; // distance between the top outside edge of the plate and the center of the first row of wells
well_dx = 9; // distance between well centers
well_dy = 9;


led_hole_diam = 4.02 + 1; // 1 mm clearance
led_h = 4.1 + 2; // 2 mm soldering tolerance
led_hole_depth = led_h + 3;
pcb_l = 100;
pcb_w = 75;
pcb_tol = 2; // allow for 2 mm space on both sides
height = 16; // make device slightly higher than normal microplate

// optional mounting holes:
mounting_holes = [[10,5],[36,64],[90,64],[90,19]];
// M2 standard bolts (with 1 mm tolerance)
screw_diam = 2 + 1;
screw_head_diam = 3.8 + 1;
screw_head_height = 3 + 1;

// positioning help: topright led center is 6 mm from each pcb edge
pcb_x = (well_x0 + well_dx*(n_cols-1)) - (pcb_l - 6);
pcb_y = (base_w - well_y0) - (pcb_w - 6);

// space for PCB components, relative to pcb
pocket_xmin = 1; // bottom left
pocket_ymin = 1;
pocket_xmax = 27;
pocket_ymax = 74;
pocket_h = 10;


// module and function definitions
module rounded_rect(dimensions, r, topleft_chamfer=0){
    w = dimensions[0];
    l = dimensions[1];
    hull(){
        // draw 4 circles in the corners
        translate([r, r])
            circle(r=r);
        if(topleft_chamfer>0){
            translate([r, w-r-topleft_chamfer])
                circle(r=r);
            translate([r+topleft_chamfer, w-r])
                circle(r=r);
        }else{
            translate([r, w-r])
                circle(r=r);
        }
        translate([l-r, r])
            circle(r=r);
        translate([l-r, w-r])
            circle(r=r);
    }
}
translate([0,base_w,height]) rotate([180,0,0])
difference(){
  union(){
    flange_h = 2.41*2;
    linear_extrude(flange_h/2)
      rounded_rect([base_w,base_l],corner_r);
    for(i = [0:10]){
      translate([flange_w*i/10, flange_w*i/10,0])
      linear_extrude(flange_h/2*(1+(i+1)/10))
        rounded_rect([base_w-2*i/10*flange_w,base_l-2*i/10*flange_w],corner_r-i/10*flange_w);
    }

    linear_extrude(height)
      translate([flange_w,flange_w])
        rounded_rect([base_w-flange_w*2,base_l-flange_w*2], corner_r-flange_w, chamfer);
        // alternatively, without the top-left corner cut off:
        // rounded_rect([base_w-flange_w*2,base_l-flange_w*2], corner_r-flange_w);
    // optional: include interuptions from the ANSI standard:
    linear_extrude(interruption_h)
      translate([interruption_x0,0])
        square([base_l-interruption_x0*2,flange_w]);
    linear_extrude(interruption_h)
      translate([interruption_x0,base_w-flange_w])
        square([base_l-interruption_x0*2,flange_w]);
  }
  
  // LED holes:
  for(y = [0:n_rows-1]){
    for(x = [4:n_cols-1]){
      translate([well_x0+x*well_dx, base_w-well_y0-y*well_dy, height/2]) cylinder(height+2,d=led_hole_diam,center=true);
    }
  }

  // pocket for PCB:
  translate([pcb_x-pcb_tol,pcb_y-pcb_tol,-1])
    linear_extrude(height-led_hole_depth+1)
      square([pcb_l+2*pcb_tol,pcb_w+2*pcb_tol]);

  // pocket for electronic components:
  translate([pcb_x,pcb_y,-1])
    linear_extrude(height-led_hole_depth+pocket_h+1)
      polygon([[pocket_xmin,pocket_ymin],
               [pocket_xmin,pocket_ymax],
               [pocket_xmax,pocket_ymax],
               [pocket_xmax,pocket_ymin]]);

  // (optional: extra space (5mm) for the longer crystal used in the prototype)
  //translate([pcb_x-5,pcb_y+pcb_w/3,-1])
  //  linear_extrude(height-led_hole_depth+pocket_h+1)
  //    square([10,pcb_w/3+2*pcb_tol]);

  // (optional: countersunk pcb mounting holes)
  for(i = [0:len(mounting_holes)-1]){
    translate([pcb_x + mounting_holes[i][0], pcb_y + mounting_holes[i][1], height/2])
      cylinder(height+2, d=screw_diam, center=true);
    translate([pcb_x + mounting_holes[i][0], pcb_y + mounting_holes[i][1], (screw_head_height+1)/2 + (height-screw_head_height)])
      cylinder(screw_head_height+1, d=screw_head_diam, center=true);
  }
}
