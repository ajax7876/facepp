#pragma once

#include "ofMain.h"
#include "Clone.h"
#include "ofxFaceTracker.h"
#include "ofxCv.h"
#include "ofxCvGrayscaleImage.h"
#include "meshForeheadAdder.h"
#include "Mandala.h"
#include "ofxDaito.h"

class ThirdEyeScene {
public:
	
	Clone clone;
	ofFbo mask, src;
	float thirdHeight, thirdMaskSize, thirdSize, strength;
	ofTexture * texy;
	float cyclopsEnergy;
	Mandala mandala;
	ofPixels maskPixels;
	
	ofPoint eyeSmoothed;
	
	// bg stuff: 
	ofPixels tempPix;
	ofTexture colorAlphaTex;
	unsigned char * colorAlphaPix;
	ofxCvGrayscaleImage maskArea;
	void generateMaskTex(ofxFaceTracker & tracker, unsigned char * colorPix){
		
		// generate a texture to be use as an alpha channel for compositing. 
		
		if (tracker.getFound()){
			
			//cout << tracker.getScale() << endl;
			ofMesh indexd = tracker.getImageMesh();
			addForheadToFaceMesh(indexd);
			vector < ofPoint > ptsToAdd;
			
			if (indexd.getVertices().size() > 0){
				
				vector< cv::Point2f > points;
				vector < ofPoint > pts = indexd.getVertices();
				for(int i = 0; i < pts.size(); i++ ){
					cv::Point2f pt;
					pt.x = pts[i].x;
					pt.y =  pts[i].y;
					points.push_back(pt);
				}

				vector<int> hull;
				cv::convexHull(cv::Mat(points), hull, CV_CLOCKWISE);

				for (int i = 0; i < hull.size(); i++){
					ptsToAdd.push_back(  ofPoint( points[hull[i]].x, points[hull[i]].y) );
				}


				maskArea.set(0);
				ofxCvBlob blob;
				for (int i = 0; i < ptsToAdd.size(); i++){
					blob.pts.push_back(	ofPoint(ptsToAdd[i].x, ptsToAdd[i].y,0));
				}
				blob.nPts = blob.pts.size();
				maskArea.drawBlobIntoMe(blob, 255);

				
				
				maskArea.dilate_3x3();
				maskArea.dilate_3x3();
				maskArea.dilate_3x3();
				maskArea.dilate_3x3();
				
				//maskArea.setROI(min.x, min.y, max.x - min.x, max.y-min.y);
				maskArea.blur(11);
				maskArea.blur(11);
				maskArea.blur(11);
				maskArea.blur(3);	
				maskArea.blur(3);	
			}
		} else {
			
			maskArea -= 1;
			maskArea.blur(11);
			maskArea.blur(11);
		}
		
		
		
		
	}
	
	
	
	void setup(int width, int height) {
		
		
		mandala.scale = 4;
		clone.setup(width, height);
		
		ofFbo::Settings settings;
		settings.width = width;
		settings.height = height;
		mask.allocate(settings);
		src.allocate(settings);
		
		thirdHeight = 0.13;
		thirdMaskSize = .4;
		thirdSize = .8;
		strength = 100;
		
		maskArea.allocate(width, height);
		colorAlphaTex.allocate(width, height, GL_RGBA);
		colorAlphaPix = new unsigned char [640*480*3];
		
		cyclopsEnergy = 0;
		
		tempPix.allocate(640,480, 4);
	}
	
	void update(ofxFaceTracker& tracker, ofImage& camImg) {
		ofTexture& cam = camImg.getTextureReference();
		
		if (tracker.getFound()){
			cyclopsEnergy = 0.99f * cyclopsEnergy + 0.01f * 1.0;
		} else {
			cyclopsEnergy = 0.85f * cyclopsEnergy + 0.15f * 0.0;
		}	
		
		//cyclopsEnergy = 0; // no cyclops. 
		
		if (tracker.getFound() == 1){
			
		
		
			int inner[] = {36, 36, 37, 37, 38, 38, 39, 39, 39, 39, 40, 40, 40, 41, 41, 36};
			int outer[] = { 0, 17, 17, 18, 18, 19, 20, 20, 21, 27, 28, 29,  2,  2,  1,  1};
			int ringPoints = 16;
			
			// third eye location is above the center of the line between the left and right eyebrows	
			// and along a line from the mouth to this point, scaled by the length of that line
			ofVec2f leftEyebrowCenter = getCentroid2D(tracker.getImageFeature(ofxFaceTracker::LEFT_EYEBROW));
			ofVec2f rightEyebrowCenter = getCentroid2D(tracker.getImageFeature(ofxFaceTracker::RIGHT_EYEBROW));
			ofVec2f thirdBase = (leftEyebrowCenter + rightEyebrowCenter) / 2;
			ofVec2f mouthCenter = getCentroid2D(tracker.getImageFeature(ofxFaceTracker::OUTER_MOUTH));
			ofVec2f thirdDirection = thirdBase - mouthCenter;
			float thirdDistance = thirdDirection.length() * thirdHeight;
			ofVec2f thirdPosition = thirdBase + thirdDirection.normalize() * thirdDistance;
			ofVec2f leftEyeCenter = getCentroid2D(tracker.getImageFeature(ofxFaceTracker::LEFT_EYE));
			ofVec2f thirdOffset = thirdPosition - leftEyeCenter;
			
			thirdSize = 1.2;
			
			ofPolyline eyeRing;
			for(int i = 0; i < ringPoints; i++) {
				ofVec2f curInner = tracker.getImagePoint(inner[i]);
				ofVec2f curOuter = tracker.getImagePoint(outer[i]);
				ofVec2f avg = curInner.interpolate(curOuter, thirdMaskSize * thirdSize);
				eyeRing.addVertex(avg.x, avg.y);
			}
			eyeRing.setClosed(true);
			eyeRing = eyeRing.getSmoothed(3);
			
			mask.begin();
			ofClear(0, 255);
			ofSetColor(255,255,255);
			ofTranslate(thirdOffset.x, thirdOffset.y);
			ofFill();
			ofBeginShape();
			for(int i = 0; i < eyeRing.size(); i++) {
				ofVertex(eyeRing[i].x, eyeRing[i].y);
			}
			ofEndShape(true);
			mask.end();
			
			mask.readToPixels(maskPixels);
			int w = maskPixels.getWidth();
			int h = maskPixels.getHeight();
			int n = w * h;
			vector<float> eyePixels;
			float mean = 0;
			for(int y = 0; y < h; y++) {
				for(int x = 0; x < w; x++) {
					if(maskPixels.getColor(x, y).getBrightness() > 128) {
						float cur = camImg.getPixelsRef().getColor(x, y).getBrightness() / 255.;
						eyePixels.push_back(cur);
						mean += cur;
					}
				}
			}
			mean /= eyePixels.size();
			
			float dev = 0;
			for(int i = 0; i < eyePixels.size(); i++) {
				dev += pow(eyePixels[i] - mean, 2);
			}
			dev = sqrt(dev / eyePixels.size());
			
			ofxDaito::bang("eyeBrightnessMean", mean);
			ofxDaito::bang("eyeBrightnessDeviation", dev);
			
			src.begin();
			
			ofClear(0, 255);
			ofTranslate(thirdPosition.x, thirdPosition.y);
			ofScale(thirdSize, thirdSize);
			ofTranslate(-leftEyeCenter.x, -leftEyeCenter.y);
			ofSetColor(255,255,255);
			cam.draw(0, 0);
			src.end();
			clone.darkPoint = thirdPosition;
			clone.setStrength(cyclopsEnergy*180);
			clone.update(src.getTextureReference(), *texy, cam, cyclopsEnergy, mask.getTextureReference());		
			eyeSmoothed = 0.95f * eyeSmoothed + 0.05 * thirdPosition;
			mandala.scale =  0.95f * mandala.scale + 0.05 * tracker.getScale();
			
			
		} else {
		
			clone.buffer.begin();
			ofSetColor(255);
			cam.draw(0,0);
			clone.buffer.end();
		}
		
		clone.buffer.readToPixels(tempPix, 0);
		
			
			
		unsigned char * colorPix = tempPix.getPixels();
		unsigned char * grayPix= maskArea.getPixels();
		for (int i = 0; i < 640*480; i++){
			colorAlphaPix[i*4+0] = colorPix[i*4+0];
			colorAlphaPix[i*4+1] = colorPix[i*4+1];
			colorAlphaPix[i*4+2] = colorPix[i*4+2];
			colorAlphaPix[i*4+3] = grayPix[i];
		}
		
		colorAlphaTex.loadData(colorAlphaPix, 640,480, GL_RGBA);
		
	}	
	
	void draw()  {
		clone.draw();
		
		ofEnableAlphaBlending();
		mandala.drawMandala(eyeSmoothed, cyclopsEnergy*0.4);
		
		ofEnableAlphaBlending();
		
		ofSetColor(255);		
		colorAlphaTex.draw(0,0);
	}
};