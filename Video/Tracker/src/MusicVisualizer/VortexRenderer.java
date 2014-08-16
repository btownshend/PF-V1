package MusicVisualizer;

import ddf.minim.AudioSource;
import processing.core.PApplet;


public class VortexRenderer extends FourierRenderer {

	int n = 48;
	float squeeze = .5f;

	float val[];

	public VortexRenderer(PApplet parent, AudioSource source) {
		super(source); 
		val = new float[n];
	}

	public void setup(PApplet parent) {
		parent.colorMode(PApplet.HSB, n, n, n);
		parent.rectMode(PApplet.CORNERS);
		parent.noStroke();
		parent.noSmooth();    
	}

	public synchronized void draw(PApplet parent	) {

		if(left != null) {  

			float t = PApplet.map((float)parent.millis(),0f, 3000f, 0f, (float)(2*Math.PI));
			float dx = parent.width / n;
			float dy = (float)(parent.height / n * .5);
			super.calc(n);

			// rotate slowly
			parent.background(0); parent.lights();
			parent.translate(parent.width/2, parent.height, -parent.width/2);
			parent.rotateZ(PApplet.HALF_PI); 
			parent.rotateY((float)(-2.2 - PApplet.HALF_PI + ((float)parent.mouseY)/parent.height * PApplet.HALF_PI));
			parent.rotateX(t);
			parent.translate(0,parent.width/4,0);
			parent.rotateX(t);

			// draw coloured slices
			for(int i=0; i < n; i++)
			{
				val[i] = PApplet.lerp(val[i], (float)Math.pow(leftFFT[i] * (i+1), squeeze), .1f);
				float x = PApplet.map(i, 0, n, parent.height, 0);
				float y = PApplet.map(val[i], 0, maxFFT, 0, parent.width/2);
				parent.pushMatrix();
				parent.translate(x, 0, 0);
				parent.rotateX((float)(Math.PI/16 * i));
				parent.fill(i, (float)(n * .7 + i * .3), n-i);
				parent.box(dy, dx + y, dx + y);
				parent.popMatrix();
			}
		}
	}
}


