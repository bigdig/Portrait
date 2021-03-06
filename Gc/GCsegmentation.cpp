#include "StdAfx.h"
#include ".\GCsegmentation.h"
#define m_K 3000.0


GCsegmentation::GCsegmentation()
:/* m_pOrginalImg(NULL)
, m_pResultImg(NULL)*/
 m_Width(0)
, m_Height(0)
, m_GmmNum(6)
, m_nDim(3)
, m_pMsIP(NULL)
, m_pG(NULL)
, m_pNodes(NULL)
, m_pMsLabels(NULL)
, m_pMsModes(NULL)
, m_pMsModePointCounts(NULL)
, m_MsRegionCount(0)
, m_MsSpatial(3)
, m_MsColor(2)
, m_MsMinRgns(10)
{/*
	m_pG     = new Graph;
	m_pNodes = new Graph::node_id[regionCount]*/;
	m_pMsIP  = new msImageProcessor;
//	CImg<unsigned char> m_pResultImg(m_Width,m_Height,1,1);

}

GCsegmentation::~GCsegmentation(void)
{
	//delete m_pG;
	//delete m_pOrginalImg;
	//delete m_pResultImg;
	delete m_pMsIP;
	delete m_pG;
	delete[] m_pNodes;
	delete[] m_pMsLabels;
	delete[] m_pMsModes;
	delete[] m_pMsModePointCounts;
}

void GCsegmentation::RegisterObserver(void(*callbackfunc)(int, string, void *), void *observer)
{
	GCSObserver gcso;
	gcso.pCallBackFunc = callbackfunc;
	gcso.pObserver = observer;
	_observers.push_back(gcso);
}

void GCsegmentation::NotifyObservers()
{
	vector<GCSObserver>::iterator itr = _observers.begin();
	while (itr != _observers.end())
	{
		((GCSObserver) *itr).pCallBackFunc(_progress, _status, ((GCSObserver) *itr).pObserver);
		itr++;
	}
}

void GCsegmentation::Initialize( CImg<unsigned char> pOrginalImg )
{
	_status = "��ʼ���ָ���";
	_progress = 0;
	NotifyObservers();

    m_pOrginalImg = pOrginalImg;
	m_Width  = m_pOrginalImg.dimx();
	m_Height = m_pOrginalImg.dimy();
	m_nDim   = m_pOrginalImg.dimv();	
   
	// * Do Mean-Shift -start- 
    unsigned char *inputImageData = new unsigned char[m_nDim * m_Width * m_Height];
	unsigned char *pUCOI = m_pOrginalImg.data;
	cimg_mapXYV(m_pOrginalImg, x, y, v)
	{
		inputImageData[m_nDim * (y * m_Width + x) + v] = *pUCOI;
		pUCOI++;
	}

	//---begin meanshift segmentation-------
	m_pMsIP->DefineImage(inputImageData, COLOR, m_Height, m_Width);
	delete[] inputImageData;

	_progress = 25;
	NotifyObservers();

	m_pMsIP->Segment(m_MsSpatial, m_MsColor, m_MsMinRgns, MED_SPEEDUP);

	_progress = 65;
	NotifyObservers();
	// * Do Mean-Shift -end-

	m_vForeMean.clear();
	m_vForeMean.resize(m_GmmNum * m_nDim);
	m_vBackMean.clear();
	m_vBackMean.resize(m_GmmNum * m_nDim);

	// Get m_pMsIP m_pOrginalImgresult
	m_MsRegionCount = m_pMsIP->GetRegionCount();
	m_pMsLabels = new int[ m_Width * m_Height ];		
	m_pMsModes  = new float[ m_MsRegionCount * m_nDim ];
	m_pMsModePointCounts = new int[ m_MsRegionCount ];
	m_MsRegionCount = m_pMsIP->GetRegions(m_pMsLabels, m_pMsModes, m_pMsModePointCounts);

	_progress = 70;
	NotifyObservers();

	CImg<float> OriginalImg_LUV(m_Width, m_Height, 1, 3);
	/*
	m_pOrginalImg.save("D:\\tempOrg.bmp");
	*/
	rgb2luv(m_pOrginalImg, OriginalImg_LUV);
	//OriginalImg_LUV = m_pOrginalImg;	

	_progress = 75;
	NotifyObservers();

	/*
    pOrginalImg.save("D:\\temp.bmp");
	OriginalImg_LUV.save("D:\\tempLUV.bmp");

	//OriginalImg_LUV.get_channel(0).save("d:\\luminace.bmp");
	//OriginalImg_LUV.get_channel(1).save("d:\\alpha.bmp");
	//OriginalImg_LUV.get_channel(2).save("d:\\Beta.bmp");
	*/

	// normalize the meanshift results
	float m_MAX = 0, m_CUR;
	int channel_offset = (int) OriginalImg_LUV.width * (int) OriginalImg_LUV.height;
	float *ptrR = OriginalImg_LUV.data;
	float *ptrG = ptrR + channel_offset;
	float *ptrB = ptrG + channel_offset;
    cimg_mapXY(OriginalImg_LUV, x0, y0) 
	{
		m_CUR = (*ptrR) * (*ptrR) + (*ptrG) * (*ptrG) + (*ptrB) * (*ptrB);

		if (m_MAX < m_CUR)
		{
			m_MAX = m_CUR;
		}

		ptrR++;
		ptrG++;
		ptrB++;
	}
	OriginalImg_LUV *= 30 / sqrtf(m_MAX);

	_progress = 80;
	NotifyObservers();
    
	memset((void *) m_pMsModes, 0, m_MsRegionCount * m_nDim * sizeof(float));
	
	ptrR = OriginalImg_LUV.data;
	ptrG = ptrR + channel_offset;
	ptrB = ptrG + channel_offset;
	cimg_mapXY(OriginalImg_LUV, x1, y1)
	{
		int index = m_pMsLabels[x1 + m_Width * y1] * m_nDim;
		m_pMsModes[index    ] += *ptrR;
		m_pMsModes[index + 1] += *ptrG;
		m_pMsModes[index + 2] += *ptrB;

		ptrR++;
		ptrG++;
		ptrB++;
	}

	_progress = 90;
	NotifyObservers();
	
	for(int i = 0; i < m_MsRegionCount * m_nDim; i++)
	{
		m_pMsModes[i] /= m_pMsModePointCounts[i / m_nDim];
	}

	RAList* raList = m_pMsIP->Get_raList();
	m_pNodes = new Graph::node_id[m_MsRegionCount];
	m_pG	 = new Graph;

	// add nodes
	for (i = 0; i < m_MsRegionCount; i++)
	{
		m_pNodes[i] = m_pG->add_node();
	}

	_progress = 95;
	NotifyObservers();

	// add weights for n-link
	double Bpq = 0.0;
	double max_Bpq = 0.0;
	double b1=0.0,b2=0.0;
	for (i = 0; i < m_MsRegionCount; i++ )
	{
		RAList *pRaList; //temp RAList
		pRaList=&raList[i];
		while (pRaList->next!=NULL)
		{
			Bpq = LAMBDA * Get_neigbour_weight(pRaList->label, pRaList->next->label);
			//b1=(double) m_pMsModePointCounts[pRaList->label]; 
			//b2=(double) m_pMsModePointCounts[pRaList->next->label];
			/*b1=std::sqrt((double) m_pMsModePointCounts[pRaList->label]); 
			b2=std::sqrt((double) m_pMsModePointCounts[pRaList->next->label]);*/
			//m_pG -> add_edge(m_pNodes[pRaList->label], m_pNodes[pRaList->next->label], Bpq*(b1/(b2+b1)), Bpq*(b2/(b1+b2)));
			m_pG -> add_edge(m_pNodes[pRaList->label], m_pNodes[pRaList->next->label], Bpq, Bpq);
			max_Bpq = max_Bpq<=Bpq? Bpq : max_Bpq;
			pRaList=pRaList->next;
		}
	}

	delete m_pMsIP;
	m_pMsIP = NULL;

	_status = "";
	_progress = 100;
	NotifyObservers();
}

CImg<unsigned char> GCsegmentation::segmentation( vector<Ptposition>FrontStroke, vector<Ptposition>BackStroke)
{
    InitiLikelihood(FrontStroke, BackStroke);
	SetLikelihood(m_pG, FrontStroke, BackStroke);
	float flow = m_pG-> maxflow();

	// Get segmentation results
	
	m_pResultImg.resize(m_Width ,m_Height, 1, 1);

	bool *bLabelTemp = new bool[m_MsRegionCount];
	for (int i = 0; i < m_MsRegionCount; i++)
	{
		// either Graph::SOURCE or Graph::SINK
		bLabelTemp[i] = (m_pG->what_segment(m_pNodes[i]) == Graph::SOURCE) ? true : false;
	}
	cimg_mapoff(m_pResultImg, offset)
	{
		m_pResultImg.data[offset] = bLabelTemp[m_pMsLabels[offset]] ? 255 : 0;
	}
	delete[] bLabelTemp;

    return m_pResultImg;
}

void GCsegmentation::InitiLikelihood(vector<Ptposition>FrontStroke, vector<Ptposition>BackStroke)
{
	vector<Ptposition>::iterator iter;
	int nMean = m_GmmNum * m_nDim, idx;

	float *FrontspData = new float[FrontStroke.size() * m_nDim];
	float *BackspData  = new float[BackStroke.size()  * m_nDim];
	float *ForeMean	   = new float[nMean];
	float *BackMean	   = new float[nMean];
	float *weight	   = new float[m_nDim];
	int   *Cluster1	   = new int[FrontStroke.size()];
	int   *Cluster2	   = new int[BackStroke.size()];
    
	// hard object constrain
	for (iter = FrontStroke.begin(), idx = 0; iter != FrontStroke.end(); iter++) 
	{
		int index = m_pMsLabels[iter->x + m_Width * iter->y] * m_nDim;
		FrontspData[idx++] = m_pMsModes[index    ];
		FrontspData[idx++] = m_pMsModes[index + 1];
		FrontspData[idx++] = m_pMsModes[index + 2];
	}

	// hard background constrain
	for (iter = BackStroke.begin(), idx = 0; iter != BackStroke.end(); iter++) 
	{
		int index = m_pMsLabels[iter->x + m_Width * iter->y] * m_nDim;
		BackspData[idx++] = m_pMsModes[index    ];
		BackspData[idx++] = m_pMsModes[index + 1];
		BackspData[idx++] = m_pMsModes[index + 2];
	}

	int ClusterNum;
	for (int w = 0; w < m_nDim; w++)
	{
		weight[w] = 1;
	}
	memset((void *) ForeMean, 0, nMean * sizeof(float));
	memset((void *) BackMean, 0, nMean * sizeof(float));

	skKMeanCluster(FrontspData, m_nDim, FrontStroke.size(), weight, ForeMean, ClusterNum, m_GmmNum, Cluster1);
	skKMeanCluster(BackspData, m_nDim, BackStroke.size(),  weight, BackMean, ClusterNum, m_GmmNum, Cluster2);

	for (w = 0; w < nMean; w++) 
	{
		m_vForeMean[w] = ForeMean[w];
		m_vBackMean[w] = BackMean[w];
	}

	delete [] Cluster1;
	delete [] Cluster2;
	delete [] weight;
	delete [] ForeMean;
	delete [] BackMean;
	delete [] FrontspData;
	delete [] BackspData;
}

void GCsegmentation::SetLikelihood(Graph* g, vector<Ptposition>FrontStroke, vector<Ptposition>BackStroke)
{
	typeTweight weights = { 0, 0 };
	for (int n = 0; n < m_MsRegionCount; n++)
	{
		switch (Get_node_type(n, FrontStroke, BackStroke))
		{
		case HARD_OBJ_LABEL:
			g->set_tweights(m_pNodes[n], m_K, 0);
			break;
		case HARD_BCK_LABEL:
			g->set_tweights(m_pNodes[n], 0, m_K);
			break;
		case UNKNOWN_LABEL:
			weights	= Get_Likelihood(n);
			g->set_tweights(m_pNodes[n], weights.w_t_link, weights.w_s_link);
			break;
		}
	}
}

int GCsegmentation::Get_node_type(int pix, vector<Ptposition>FrontStroke, vector<Ptposition>BackStroke)
{
	Ptposition pttemp;
	vector<Ptposition>::iterator iter1=FrontStroke.begin();
	vector<Ptposition>::iterator iter2=BackStroke.begin();

	// hard object constrain
	for (;iter1!=FrontStroke.end();iter1++) 
	{
		pttemp=*iter1;
		if ( m_pMsLabels[pttemp.x+m_Width*pttemp.y] == pix )
		{
			return HARD_OBJ_LABEL;
		}
	}

	// hard background constrain
	for (;iter2!=BackStroke.end();iter2++) 
	{
		pttemp=*iter2;
		if (  m_pMsLabels[pttemp.x+m_Width*pttemp.y] == pix )
		{
			return HARD_BCK_LABEL;
		}
	}

	// unknown label
	return UNKNOWN_LABEL;
}

typeTweight GCsegmentation::Get_Likelihood(int x)
{
	typeTweight t = { 0.0f, 0.0f };

	float p1 = 1000000.0f;
	float p2 = 1000000.0f;
	float *dataTemp = new float[m_nDim];

	memcpy((void *) dataTemp, (const void *) &m_pMsModes[x * m_nDim], m_nDim * sizeof(float));

	for (int i = 0; i < m_GmmNum; i++)
	{
		float t1 = 0.0f;
		float t2 = 0.0f;

		int index = i * m_nDim;
		for (int dim = 0; dim < m_nDim; dim++)
		{	
			t1 += powf((dataTemp[dim] - m_vForeMean[index + dim]), 2);
			t2 += powf((dataTemp[dim] - m_vBackMean[index + dim]), 2);
		}

		if (p1 > t1)
		{
			p1 = t1;
		}
		if (p2 > t2)
		{
			p2 = t2;
		}
	}

	delete [] dataTemp;

	p1 = sqrtf(p1);
	p2 = sqrtf(p2);

	t.w_s_link = p1 / (p1 + p2);
	t.w_t_link = p2 / (p1 + p2);

	return t;
}

float GCsegmentation::Get_neigbour_weight(int num_p, int num_q)
{
	/*int Ip1 = m_pMsModes[num_p*m_nDim+0]; 
	int Ip2 = m_pMsModes[num_p*m_nDim+1]; 
	int Ip3 = m_pMsModes[num_p*m_nDim+2]; 

	int Iq1 = m_pMsModes[num_q*m_nDim+0]; 
	int Iq2 = m_pMsModes[num_q*m_nDim+1]; 
	int Iq3 = m_pMsModes[num_q*m_nDim+2]; 
	*/
	float sum = 0.0f;

	int idx_p = num_p * m_nDim;
	int idx_q = num_q * m_nDim;
	for (int dim = 0; dim < m_nDim; dim++)
	{
		float dpq = m_pMsModes[idx_p + dim] - m_pMsModes[idx_q + dim];
		sum += dpq * dpq;
	}

	return 1 / (sum + 1);
	//return 1/(pow(pow(Ip1-Iq1,2) + pow(Ip2-Iq2,2) + pow(Ip3-Iq3,2), 1/1)+1);
}

void GCsegmentation::rgb2luv(CImg<unsigned char> &src, CImg<float> &dst)
{
	int height =src.dimx();
	int width  =src.dimy();
	const double XYZ[3][3] = {	{  0.4125,  0.3576,  0.1804 },
								{  0.2125,  0.7154,  0.0721 },
								{  0.0193,  0.1192,  0.9502 }	};
	const double Yn			= 1.00000;
	const double Un_prime	= 0.19784977571475;
	const double Vn_prime	= 0.46834507665248;
	const double Lt			= 0.008856;

	cimg_mapXY(src, i, j)
	{
		double	x, y, z, L0, u_prime, v_prime, constant;
		double luvLVal,luvUVal,luvVVal;

		//convert RGB to XYZ...
		x		= XYZ[0][0]*( src(i,j,0) ) + XYZ[0][1]*(src(i,j,1)) + XYZ[0][2]*(src(i,j,2));
		y		= XYZ[1][0]*( src(i,j,0) ) + XYZ[1][1]*(src(i,j,1)) + XYZ[1][2]*(src(i,j,2));
		z		= XYZ[2][0]*( src(i,j,0) ) + XYZ[2][1]*(src(i,j,1)) + XYZ[2][2]*(src(i,j,2));

		//convert XYZ to LUV...

		//compute L*
		L0		= y / (255.0 * Yn);
		if(L0 > Lt)
			luvLVal	= (float)(116.0 * (pow(L0, 1.0/3.0)) - 16.0);
		else
			luvLVal	= (float)(903.3 * L0);

		//compute u_prime and v_prime
		constant	= x + 15 * y + 3 * z;
		if(constant != 0)
		{
			u_prime	= (4 * x) / constant;
			v_prime = (9 * y) / constant;
		}
		else
		{
			u_prime	= 4.0;
			v_prime	= 9.0/15.0;
		}

		//compute u* and v*
		luvUVal = (float) (13 * luvLVal * (u_prime - Un_prime));
		luvVVal = (float) (13 * luvLVal * (v_prime - Vn_prime));

		dst(i, j, 0) = luvLVal;
		dst(i, j, 1) = luvUVal;
		dst(i, j, 2) = luvVVal;		 
	}
	//done.
	return;
}

//
//bool GCsegmentation::valid_neighbour(int wid, int heig)
//{
//	if ((wid>=0) && (wid<Width) && (heig>=0) && (heig<Height))
//		return true;
//	else
//		return false;
//}