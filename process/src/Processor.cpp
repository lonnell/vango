#include "Processor.h"

using namespace cv;

void Processor::initialize(std::string imgFile, std::string styleFile, bool verby){
    verbose = verby;
    
    // load image
    if(verbose){
        std::cout << std::endl << "Reading image:  " << imgFile << std::endl;
    }

    image = imread(imgFile, CV_LOAD_IMAGE_COLOR);    
    if(!image.data){
        std::cout << "No image data :(" << std::endl;
        return;
    }
    Size imgSize = image.size();
 
    // load style
    if(verbose){
        std::cout << "  Image size: " << imgSize << std::endl << std::endl;
        std::cout << "With style: " << styleFile << std::endl;
    }
    YAML::Node styleNode = YAML::LoadFile(styleFile);
    canvStyle = styleNode.as<CanvasStyle>();
    
    canvas.width = imgSize.width * canvStyle.canvasScale;
    canvas.height = imgSize.height * canvStyle.canvasScale;

    for(int i = 0; i < canvStyle.layers.size(); ++i){
        Layer l;
        canvas.layers.push_back(l);
        blurimages.push_back(image.clone());
    }

    if(verbose){
        std::cout << "Reading canvas -done- " << std::endl;
        std::cout << "  canvasScale: " << canvStyle.canvasScale << std::endl;
        std::cout << "  canvasSize: " << canvas.width << " x " << canvas.height << std::endl;
        std::cout << "  num layers: " << canvas.layers.size() << std::endl;     
        std::cout << std::endl << "End initialization............" << std::endl << std::endl;
    }
}

void Processor::initialize(std::string imgFile, std::string styleFile){
    initialize(imgFile, styleFile, false);
}

void Processor::processImage() {    
    for(int i = 0; i < canvas.layers.size(); ++i){
        LayerStyle& lstyle = canvStyle.layers[i];
        Layer& layer = canvas.layers[i];
        if(verbose){
            std::cout << "On Layer " << i << std::endl;
            std::cout << "  regenWidth: " << lstyle.regenWidth << std::endl;
            std::cout << "  regenMaskWidth: " << lstyle.regenMaskWidth << std::endl;
            std::cout << "  avgBrushWidth: " << lstyle.avgBrushWidth << std::endl;
            std::cout << "  varBrushWidth: " << lstyle.varBrushWidth << std::endl;
            std::cout << "  opacity: " << lstyle.opacity << std::endl;
            std::cout << "  strengthThreshold: " << lstyle.strengthThreshold << std::endl;
            std::cout << "  strengthNeighborhood: " << lstyle.strengthNeighborhood << std::endl;             
        }      

        int kernelSize = (int)(1.0 * lstyle.avgBrushWidth/2.0); 
        // needs to be positively odd
        if(kernelSize/2.0 == std::floor(kernelSize/2.0))
            kernelSize ++;

        if(verbose)
            std::cout << "  kernelSize: " << kernelSize << std::endl;

        if(i != canvas.layers.size()-1) // absolutely do not blur the top image jeez 
            GaussianBlur(image, blurimages[i], Size(kernelSize, kernelSize), 0, 0, BORDER_DEFAULT);
 
        buildStrokes(layer, lstyle, blurimages[i]);
        angleStrokes(layer, lstyle);
        clipStrokes(layer, lstyle);
        colorStrokes(layer, lstyle, blurimages[i]);

        if(verbose)
            std::cout << std::endl;
    }
}

void Processor::saveToFile(std::string outFile){
    if(verbose){
        std::cout << "writing to file... " << outFile << std::endl;
    }
    YAML::Emitter yout;
    YAML::convert<Canvas> yconv;
    YAML::Node canvasNode = yconv.encode(canvas);
    yout << canvasNode;
    
    std::ofstream fout; 
    fout.open(outFile.c_str());
    fout << yout.c_str();
    fout.close();
    if(verbose){

        std::cout << "finished writing" << std::endl;
    }
}


// Get anchors, set width + opacity
void Processor::buildStrokes(Layer& layer, LayerStyle& lstyle, cv::Mat& blurimg){
    int k = .25 * canvas.width * canvas.height; // why not, obviously a factor of image size
    int stopthresh = k * .001;
    int countstop = 0;    
    double wrhalf = lstyle.regenWidth / 2.0;


    if(verbose){
        std::cout << "buildingStrokes!" << std::endl;
        std::cout << "  k: " << k << std::endl;
        std::cout << "  stopthresh: " << stopthresh << std::endl;
    }

    // Create a mask to store placed brushstrokes and allowed placement locations
    Scalar s = 0;
    Mat maskimg(canvas.height, canvas.width, CV_8UC1, s);     
    Size maskSize = maskimg.size();
    // if regenMaskWidth == 0, this is the bottom layer and should subsequently be fully covered
    if (lstyle.regenMaskWidth > .00000001){
        createRegenMask(maskimg, blurimg, lstyle.regenMaskWidth);

    }

    for(int i = 0; i < k; ++i){
        if(countstop > stopthresh){
            if(verbose){
                std::cout << "breaking: reached stopthresh" << std::endl;
            }
            break;
        }
        
        int r = rand()%maskSize.height;
        int c = rand()%maskSize.width;
        
        int rs = std::max(0, (int)(r-wrhalf));
        int re = std::min((int)(r+wrhalf), maskSize.height);
        int cs = std::max(0, (int)(c-wrhalf));
        int ce = std::min((int)(c+wrhalf), maskSize.width);

        if(verbose){
            std::cout << "test position: " << r << ", " << c << std::endl;
            std::cout << "  r: [" << rs << ", " << re << "]" << std::endl;
            std::cout << "  c: [" << cs << ", " << ce << "]" << std::endl;
        }
        
        if(countNonZero(maskimg(Range(rs, re), Range(cs, ce))) > 0){
            countstop++;
            if(verbose){
                std::cout << "  rejected" << std::endl;
                std::cout << "  countstop: " << countstop << std::endl;
            }
            continue;
        }
        
        // valid position
    
        countstop = 0;
        maskimg.at<uchar>(r, c) = 255;
        
        Brushstroke b;
        makeDummyStroke(b, Point2d(c, r), lstyle.avgBrushWidth, lstyle.varBrushWidth, lstyle.opacity);

        if(verbose){
            std::cout << "  New stroke: " << std::endl;
            std::cout << "      anchor: " << b.anchor << std::endl;
            std::cout << "      width: " << b.width << std::endl;
            std::cout << "      opacity: " << b.opacity << std::endl;
        }

        layer.strokes.push_back(b);
    }

    if(verbose){
        std::cout << "-------------Postprocessing stroke generation----------------" << std::endl;
    }
        
    for(int r = 0; r < maskSize.height; ++r){
        for(int c = 0; c < maskSize.width; ++c){
       
            int rs = std::max(0, (int)(r-wrhalf)); 
            int re = std::min((int)(r+wrhalf), maskSize.height);
            int cs = std::max(0, (int)(c-wrhalf));
            int ce = std::min((int)(c+wrhalf), maskSize.width);

            if (countNonZero(maskimg(Range(rs, re), Range(cs, ce))) >= 1){
                continue;
            }
            // valid point
 
            maskimg.at<uchar>(r,c) = 255; 

            Brushstroke b;
            makeDummyStroke(b, Point2d(c, r), lstyle.avgBrushWidth, lstyle.varBrushWidth, lstyle.opacity);

            if(verbose){
                std::cout << "  New stroke: " << std::endl;
                std::cout << "      anchor: " << b.anchor << std::endl;
                std::cout << "      width: " << b.width << std::endl;
                std::cout << "      opacity: " << b.opacity << std::endl;
            }

            layer.strokes.push_back(b);
        }
    }
   
    if(verbose){
        std::cout << "Done building strokes..." << std::endl;
        displayImage(maskimg, "Brush anchors");
    }
}

void Processor::angleStrokes(Layer& layer, LayerStyle& lstyle){


}

void Processor::clipStrokes(Layer& layer, LayerStyle& lstyle){


}

void Processor::colorStrokes(Layer& layer, LayerStyle& lstyle, cv::Mat& bimage){
    // for now, just use the color at the anchor point
    
    if(verbose){
        std::cout << "Started coloring strokes" << std::endl;
    }

    Vec3b col; 
    double scale = canvStyle.canvasScale;
    for(int i = 0; i < layer.strokes.size(); ++i){
        Brushstroke& stroke = layer.strokes[i];
        // use blurred image when getting color 
        col = bimage.at<Vec3b>(Point(stroke.anchor.x / scale, stroke.anchor.y / scale));
        stroke.color = BGR_TO_RGBDOUBLE(col);

        if(verbose){
            std::cout << " Set color at stroke " << stroke.anchor << " to " << stroke.color << std::endl;
        }
    }
    if(verbose){
        std::cout << "Finished coloring strokes" << std::endl;
    }
}

void Processor::makeDummyStroke(Brushstroke& stroke, Point2d ankh, double avgWb, double dWb, double opac){
    stroke.anchor = ankh;
    stroke.width = avgWb + (2.0*(double)rand()/(RAND_MAX) - 1.0)*dWb;
    stroke.opacity = opac; 

    // dummy values for other parameters
    stroke.angle = .78;
    stroke.strength = 0;
    stroke.length1 = 2;
    stroke.length2 = 2;
    stroke.color = Vec3d(.5, .5, .5);

}

void Processor::createRegenMask(cv::Mat& mask, cv::Mat& blurimg, double rmaskwidth){
    Mat grayimg;
    Mat threshimg; 
    cvtColor(blurimg, grayimg, CV_RGB2GRAY); // intensity image
    Mat gradX, gradY;
    Mat grad; 
    double kernelsize = 3;

    if(verbose){
        std::cout << "creating RegenMask" << std::endl;
        std::cout << "  kernelsize: " << kernelsize << std::endl;
    }

    std::cout << "scale : " << canvStyle.canvasScale << std::endl;
    Sobel(grayimg, gradX, CV_32F, 1, 0, kernelsize, 1.0, 0, BORDER_DEFAULT); 
    Sobel(grayimg, gradY, CV_32F, 0, 1, kernelsize, 1.0, 0, BORDER_DEFAULT);
    magnitude(gradX, gradY, grad);
    
    double threshval = 10.0; 

    if(verbose){
        std::cout << "  thresholding..." << std::endl;
        std::cout << "  threshvalue: " << threshval << std::endl;
    }
    
    threshold(grad, threshimg, threshval, 255, 1);
    threshimg.convertTo(threshimg, CV_8U, 1);
    if(verbose){
        displayImage(threshimg, "mask before hole-filling");
    }
    
    Mat element = getStructuringElement(MORPH_ELLIPSE, Size(3, 3));
    morphologyEx(threshimg, threshimg,  MORPH_OPEN, element);
    
    if(verbose){
        displayImage(threshimg, "mask after hole-filling");
    }

    resize(threshimg, mask, mask.size(), 0, 0, INTER_LINEAR);
    //dilate(mask, mask, Mat(), Point(-1, -1), (int)rmaskwidth);
    
} 

void Processor::displayImage(cv::Mat& img, std::string windowName){
    namedWindow(windowName, WINDOW_NORMAL);
    imshow(windowName, img);
    waitKey(0);
}







void Processor::ignorethisblurImage(double kernelwidth, double kernelheight){
    Mat blurimg = image.clone();
    //for (int i = 1; i < max_kernel_length; i += 2){
        // Size(w,h): size of the kernel to be used, with w, h odd and positive
        // stdevx, stddevy... if pass in 0, calculated from kernel size
        GaussianBlur(image, blurimg, Size(kernelwidth, kernelheight), 0, 0); 
    //}    
    image = blurimg;

}

void Processor::ignorethisdoSobel(int kernelsize){
    Mat grayimg;
    cvtColor(image, grayimg, CV_RGB2GRAY);
    Mat gradX, gradY;
    Mat absgradX, absgradY;
    Mat grad;
    // Sobel(src, dst, int ddepth, int dx, int dy, int ksize, double scale, double delta, int borderType)
    // ddepth: output image depth, same as source if set to -1
    // xorder: order of derivative x
    // yorder: order of derivative y
    // ksize: size of extended Sobel kernel
    // scale: optional scaling of output
    // delta: optional value to add to results
    // borderType: extrapolation method at borders
    Sobel(grayimg, gradX, CV_32F, 1, 0, kernelsize, 1.0, 0, BORDER_DEFAULT);
    //convertScaleAbs(gradX, absgradX);
    //absgradX = abs(gradX);
    Sobel(grayimg, gradY, CV_32F, 0, 1, kernelsize, 1.0, 0, BORDER_DEFAULT);
    //convertScaleAbs(gradY, absgradY);
   // absgradY = abs(gradY);

    
    // Note that it matters whether you're dealing with bytes (gray) or ints
    std::cout << (absgradX.size == absgradY.size) << std::endl; 
    std::cout << gradX.depth() << std::endl;
    std::cout << gradY.depth() << std::endl;
    std::cout << absgradX.depth() << std::endl;
    std::cout << absgradY.depth() << std::endl;



//    magnitude(absgradX, absgradY, grad);
    magnitude(gradX, gradY, grad);

    namedWindow("gradWind", CV_WINDOW_AUTOSIZE);
    imshow("gradWind", grad);
    waitKey(0);


}


void Processor::ignorethisdisplay(std::string windowName){
    namedWindow(windowName, WINDOW_NORMAL);
    imshow(windowName, image);

    waitKey(0);
    return;
}


void Processor::ignorethisplaceStrokes(){
    // create binary matrix of size image
    // actually... technically... of size image * scale... :(    
    // brush width, wr. etc. all in terms of canvas size... 

    int k = 5000; // try k random points
    int stopthresh = 100; // if failed to place stopthresh times, stop algorithm 
    double scale = 1;
    double wr = 10; 
    int countstop = 0;

    Size imgSize = image.size();
    Scalar s = 0;
    Mat maskimg(imgSize.height*scale, imgSize.width*scale, CV_8UC1, s);
    Size maskSize = maskimg.size();

    std::vector<Brushstroke> strokes;
    srand(time(NULL));

    std::cout << "image size: " << imgSize << std::endl;
    std::cout << "canvas Size: " << maskSize << std::endl;
    //std::cout << countNonZero(maskimg) << std::endl;


    for (int i = 0; i < k; ++i){
        if(countstop > stopthresh){
            std::cout << "breaking: reached stopthresh" << std::endl;
            break;
        }
        //std::cout << countstop << std::endl;
        int r = rand()%maskSize.height;
        int c = rand()%maskSize.width;                  
       
        int rs = std::max(0, (int)(r-wr/2.0)); 
        int re = std::min((int)(r+wr/2.0), maskSize.height);
        int cs = std::max(0, (int)(c-wr/2.0));
        int ce = std::min((int)(c+wr/2.0), maskSize.width);

        //std::cout << "trying " << r << ", " << c << std::endl;
        if (countNonZero(maskimg(Range(rs, re), Range(cs, ce))) >= 1){
            //std::cout << "rejected " << std::endl;
            countstop++;
            continue;
        }
        countstop = 0;
        //std::cout << "setting " << std::endl;
        maskimg.at<uchar>(r,c) = 255; 

        Brushstroke b;
        b.anchor = Point2d(c, r);
        strokes.push_back(b);
                


        
        //std::cout << "r: " << rs << ", " << re << std::endl;
        //std::cout << "c: " << cs << ", " << ce << std::endl;


        //maskimg.at<uchar>(rs, cs) = 255;
        //maskimg.at<uchar>(rs, ce) = 255;
        //maskimg.at<uchar>(re, cs) = 255;
        //maskimg.at<uchar>(re, ce) = 255;

        //Vec3b col = image.at<Vec3b>(Point(x/2,y/2));
    
    }
  
    //std::cout << countNonZero(maskimg) << std::endl;

    namedWindow("gradWind", CV_WINDOW_NORMAL);
    imshow("gradWind", maskimg);
    waitKey(0); 

    //std::cout << "postprocessing... fill in the holes" << std::endl;
    
    for(int r = 0; r < maskSize.height; ++r){
        for(int c = 0; c < maskSize.width; ++c){
       
            int rs = std::max(0, (int)(r-wr/2.0)); 
            int re = std::min((int)(r+wr/2.0), maskSize.height);
            int cs = std::max(0, (int)(c-wr/2.0));
            int ce = std::min((int)(c+wr/2.0), maskSize.width);

            //std::cout << "trying " << r << ", " << c << std::endl;
            if (countNonZero(maskimg(Range(rs, re), Range(cs, ce))) >= 1){
                //std::cout << "rejected " << std::endl;
                continue;
            }
            //std::cout << "setting " << r << ", " << c << std::endl;
            maskimg.at<uchar>(r,c) = 255; 

            Brushstroke b;
            b.anchor = Point2d(c, r);
            strokes.push_back(b); 

        }
    }

    namedWindow("pWind", CV_WINDOW_NORMAL);
    imshow("pWind", maskimg);
    waitKey(0); 

   



    for (int i = 0; i < imgSize.height; ++i){
        for(int j = 0; j < imgSize.width; ++j){
    
            if(maskimg.at<uchar>(i*2.0, j*2.0) > 100){
                //image.at<Vec3b>(i,j) = {0, 0, 0};
            }

        }
    }

//    display();


/*
    for (int i = 0; i < imgSize.height; ++i){
        for(int j = 0; j < imgSize.width; ++j){

            Vec3b col = image.at<Vec3b>(Point(j,i));
                        
            std::cout << (int)col[0] << ", " << (int)col[1] << ", " << (int)col[2] << std::endl;


        }
    }  
*/

    std::cout << "strokes: " << strokes.size() << std::endl;


    // Oh gods okay let's add to this monstrosity... 
    // fill filler values for now... 
    
    // angles!
    
    std::cout << "starting brush setting" << std::endl;
    for (int i = 0; i < strokes.size(); ++i){
        Brushstroke& stroke = strokes[i];
        stroke.angle = .78;
        stroke.strength = 0;
        stroke.length1 = 4;
        stroke.length2 = 4;
        stroke.width = 8;
        stroke.opacity = 1.0;
        //std::cout << "try to access color at " << stroke.anchor.x/scale << ", " << stroke.anchor.y/scale << std::endl;
        Vec3b col = image.at<Vec3b>(Point(stroke.anchor.x/scale, stroke.anchor.y/scale));
        //std::cout << "original color: " << (int)col[0] << ", " << (int)col[1] << ", " << (int)col[2] << std::endl;
        stroke.color = Vec3d(col[2]/255.0, col[1]/255.0, col[0]/255.0);
        //std::cout << stroke.color[0] << ", " << stroke.color[1] << ", " << stroke.color[2] << std::endl;
         //Vec3b col = image.at<Vec3b>(Point(x/2,y/2));
        if (stroke.color[0] < .000001 && stroke.color[1] < .0000001 && stroke.color[2] < .0000001){
            std::cout << "0 at: " << stroke.anchor.x << ", " << stroke.anchor.y << std::endl;
        }
    }

    std::cout << "ending brush setting" << std::endl;

    Canvas canvas; 
    canvas.width = maskSize.width;
    canvas.height = maskSize.height;
    
    std::vector<Layer> layers; 
    Layer onlyLayer;
    onlyLayer.strokes = strokes;

    layers.push_back(onlyLayer);
    canvas.layers = layers;
    

    YAML::Emitter yout; 
    //YAML::Node canvasNode = YAML::Load(canvas);
    YAML::convert<Canvas> ccon;
    YAML::Node canvasNode = ccon.encode(canvas);
        

    yout << canvasNode;


    std::ofstream fout;
    fout.open("testthing.yaml");
    fout << yout.c_str();    
    fout.close();

    //out << canvas;
    //YAML::Node canvasNode; 
        

}




