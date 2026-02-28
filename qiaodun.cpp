// 这是使用occt构建流线型桥墩模型的代码
void COccTryView::OnThru() {
  // AIS_ListOfInteractive aList;
  // myAISContext->DisplayedObjects(aList);
  // AIS_ListIteratorOfListOfInteractive aListIterator;
  // for (aListIterator.Initialize(aList); aListIterator.More();
  // aListIterator.Next()) { 	myAISContext->Remove(aListIterator.Value(),
  //false);
  // }

  //******流线型托盘******
  gp_Pnt P1(16, -14, 0);
  gp_Pnt P2(30, 0, 0);
  gp_Pnt P3(16, 14, 0);
  gp_Pnt P1_1(-16, -14, 0);
  gp_Pnt P2_1(-30, 0, 0);
  gp_Pnt P3_1(-16, 14, 0);
  gp_Pnt P1_2(2, -14, 0);
  gp_Pnt P2_2(1.29, -13.74, 0);
  gp_Pnt P3_2(1, -13, 0);
  gp_Pnt P1_3(-2, -14, 0);
  gp_Pnt P2_3(-1.29, -13.74, 0);
  gp_Pnt P3_3(-1, -13, 0);
  gp_Pnt P3_4(0, -12, 0);
  gp_Pnt P1_5(2, 14, 0);
  gp_Pnt P2_5(1.29, 13.74, 0);
  gp_Pnt P3_5(1, 13, 0);
  gp_Pnt P1_6(-2, 14, 0);
  gp_Pnt P2_6(-1.29, 13.74, 0);
  gp_Pnt P3_6(-1, 13, 0);
  gp_Pnt P3_7(0, 12, 0);
  Handle(Geom_TrimmedCurve) anArcOfCircle1 =
      GC_MakeArcOfCircle(P1_1, P2_1, P3_1);
  Handle(Geom_TrimmedCurve) aSegment1 = GC_MakeSegment(P3_1, P1_6);
  Handle(Geom_TrimmedCurve) anArcOfCircle2 =
      GC_MakeArcOfCircle(P1_6, P2_6, P3_6);
  Handle(Geom_TrimmedCurve) anArcOfCircle3 =
      GC_MakeArcOfCircle(P3_6, P3_7, P3_5);
  Handle(Geom_TrimmedCurve) anArcOfCircle4 =
      GC_MakeArcOfCircle(P3_5, P2_5, P1_5);
  Handle(Geom_TrimmedCurve) aSegment2 = GC_MakeSegment(P1_5, P3);
  Handle(Geom_TrimmedCurve) anArcOfCircle5 = GC_MakeArcOfCircle(P3, P2, P1);
  Handle(Geom_TrimmedCurve) aSegment3 = GC_MakeSegment(P1, P1_2);
  Handle(Geom_TrimmedCurve) anArcOfCircle6 =
      GC_MakeArcOfCircle(P1_2, P2_2, P3_2);
  Handle(Geom_TrimmedCurve) anArcOfCircle7 =
      GC_MakeArcOfCircle(P3_2, P3_4, P3_3);
  Handle(Geom_TrimmedCurve) anArcOfCircle8 =
      GC_MakeArcOfCircle(P3_3, P2_3, P1_3);
  Handle(Geom_TrimmedCurve) aSegment4 = GC_MakeSegment(P1_3, P1_1);
  TopoDS_Edge anEdge1 = BRepBuilderAPI_MakeEdge(anArcOfCircle1);
  TopoDS_Edge anEdge2 = BRepBuilderAPI_MakeEdge(aSegment1);
  TopoDS_Edge anEdge3 = BRepBuilderAPI_MakeEdge(anArcOfCircle2);
  TopoDS_Edge anEdge4 = BRepBuilderAPI_MakeEdge(anArcOfCircle3);
  TopoDS_Edge anEdge5 = BRepBuilderAPI_MakeEdge(anArcOfCircle4);
  TopoDS_Edge anEdge6 = BRepBuilderAPI_MakeEdge(aSegment2);
  TopoDS_Edge anEdge7 = BRepBuilderAPI_MakeEdge(anArcOfCircle5);
  TopoDS_Edge anEdge8 = BRepBuilderAPI_MakeEdge(aSegment3);
  TopoDS_Edge anEdge9 = BRepBuilderAPI_MakeEdge(anArcOfCircle6);
  TopoDS_Edge anEdge10 = BRepBuilderAPI_MakeEdge(anArcOfCircle7);
  TopoDS_Edge anEdge11 = BRepBuilderAPI_MakeEdge(anArcOfCircle8);
  TopoDS_Edge anEdge12 = BRepBuilderAPI_MakeEdge(aSegment4);
  TopoDS_Wire aWire =
      BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3, anEdge4);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge5);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge6);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge7);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge8);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge9);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge10);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge11);
  aWire = BRepBuilderAPI_MakeWire(aWire, anEdge12);
  Handle(AIS_Shape) sec1 = new AIS_Shape(aWire);
  // myAISContext->Display(sec1,false);
  P1.SetCoord(24, -15, 27.5);
  P2.SetCoord(39, 0, 27.5);
  P3.SetCoord(24, 15, 27.5);
  P1_1.SetCoord(-24, -15, 27.5);
  P2_1.SetCoord(-39, 0, 27.5);
  P3_1.SetCoord(-24, 15, 27.5);
  P1_2.SetCoord(2, -15, 27.5);
  P2_2.SetCoord(1.29, -14.71, 27.5);
  P3_2.SetCoord(1, -14, 27.5);
  P1_3.SetCoord(-2, -15, 27.5);
  P2_3.SetCoord(-1.29, -14.71, 27.5);
  P3_3.SetCoord(-1, -14, 27.5);
  P3_4.SetCoord(0, -13, 27.5);
  P1_5.SetCoord(2, 15, 27.5);
  P2_5.SetCoord(1.29, 14.71, 27.5);
  P3_5.SetCoord(1, 14, 27.5);
  P1_6.SetCoord(-2, 15, 27.5);
  P2_6.SetCoord(-1.29, 14.71, 27.5);
  P3_6.SetCoord(-1, 14, 27.5);
  P3_7.SetCoord(0, 13, 27.5);
  anArcOfCircle1 = GC_MakeArcOfCircle(P1_1, P2_1, P3_1);
  aSegment1 = GC_MakeSegment(P3_1, P1_6);
  anArcOfCircle2 = GC_MakeArcOfCircle(P1_6, P2_6, P3_6);
  anArcOfCircle3 = GC_MakeArcOfCircle(P3_6, P3_7, P3_5);
  anArcOfCircle4 = GC_MakeArcOfCircle(P3_5, P2_5, P1_5);
  aSegment2 = GC_MakeSegment(P1_5, P3);
  anArcOfCircle5 = GC_MakeArcOfCircle(P3, P2, P1);
  aSegment3 = GC_MakeSegment(P1, P1_2);
  anArcOfCircle6 = GC_MakeArcOfCircle(P1_2, P2_2, P3_2);
  anArcOfCircle7 = GC_MakeArcOfCircle(P3_2, P3_4, P3_3);
  anArcOfCircle8 = GC_MakeArcOfCircle(P3_3, P2_3, P1_3);
  aSegment4 = GC_MakeSegment(P1_3, P1_1);
  anEdge1 = BRepBuilderAPI_MakeEdge(anArcOfCircle1);
  anEdge2 = BRepBuilderAPI_MakeEdge(aSegment1);
  anEdge3 = BRepBuilderAPI_MakeEdge(anArcOfCircle2);
  anEdge4 = BRepBuilderAPI_MakeEdge(anArcOfCircle3);
  anEdge5 = BRepBuilderAPI_MakeEdge(anArcOfCircle4);
  anEdge6 = BRepBuilderAPI_MakeEdge(aSegment2);
  anEdge7 = BRepBuilderAPI_MakeEdge(anArcOfCircle5);
  anEdge8 = BRepBuilderAPI_MakeEdge(aSegment3);
  anEdge9 = BRepBuilderAPI_MakeEdge(anArcOfCircle6);
  anEdge10 = BRepBuilderAPI_MakeEdge(anArcOfCircle7);
  anEdge11 = BRepBuilderAPI_MakeEdge(anArcOfCircle8);
  anEdge12 = BRepBuilderAPI_MakeEdge(aSegment4);
  TopoDS_Wire bWire =
      BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3, anEdge4);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge5);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge6);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge7);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge8);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge9);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge10);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge11);
  bWire = BRepBuilderAPI_MakeWire(bWire, anEdge12);
  Handle(AIS_Shape) sec2 = new AIS_Shape(bWire);
  // myAISContext->Display(sec2,false);
  P1.SetCoord(17.88, -14.095, 13.75);
  P2.SetCoord(31.905, 0, 13.75);
  P3.SetCoord(17.88, 14.095, 13.75);
  P1_1.SetCoord(-17.88, -14.095, 13.75);
  P2_1.SetCoord(-31.905, 0, 13.75);
  P3_1.SetCoord(-17.88, 14.095, 13.75);
  P1_2.SetCoord(2, -14.095, 13.75);
  P2_2.SetCoord(1.29, -13.8, 13.75);
  P3_2.SetCoord(1, -13.1, 13.75);
  P1_3.SetCoord(-2, -14.095, 13.75);
  P2_3.SetCoord(-1.29, -13.8, 13.75);
  P3_3.SetCoord(-1, -13.1, 13.75);
  P3_4.SetCoord(0, -12.1, 13.75);
  P1_5.SetCoord(2, 14.095, 13.75);
  P2_5.SetCoord(1.29, 13.8, 13.75);
  P3_5.SetCoord(1, 13.1, 13.75);
  P1_6.SetCoord(-2, 14.095, 13.75);
  P2_6.SetCoord(-1.29, 13.8, 13.75);
  P3_6.SetCoord(-1, 13.1, 13.75);
  P3_7.SetCoord(0, 12.1, 13.75);
  anArcOfCircle1 = GC_MakeArcOfCircle(P1_1, P2_1, P3_1);
  aSegment1 = GC_MakeSegment(P3_1, P1_6);
  anArcOfCircle2 = GC_MakeArcOfCircle(P1_6, P2_6, P3_6);
  anArcOfCircle3 = GC_MakeArcOfCircle(P3_6, P3_7, P3_5);
  anArcOfCircle4 = GC_MakeArcOfCircle(P3_5, P2_5, P1_5);
  aSegment2 = GC_MakeSegment(P1_5, P3);
  anArcOfCircle5 = GC_MakeArcOfCircle(P3, P2, P1);
  aSegment3 = GC_MakeSegment(P1, P1_2);
  anArcOfCircle6 = GC_MakeArcOfCircle(P1_2, P2_2, P3_2);
  anArcOfCircle7 = GC_MakeArcOfCircle(P3_2, P3_4, P3_3);
  anArcOfCircle8 = GC_MakeArcOfCircle(P3_3, P2_3, P1_3);
  aSegment4 = GC_MakeSegment(P1_3, P1_1);
  anEdge1 = BRepBuilderAPI_MakeEdge(anArcOfCircle1);
  anEdge2 = BRepBuilderAPI_MakeEdge(aSegment1);
  anEdge3 = BRepBuilderAPI_MakeEdge(anArcOfCircle2);
  anEdge4 = BRepBuilderAPI_MakeEdge(anArcOfCircle3);
  anEdge5 = BRepBuilderAPI_MakeEdge(anArcOfCircle4);
  anEdge6 = BRepBuilderAPI_MakeEdge(aSegment2);
  anEdge7 = BRepBuilderAPI_MakeEdge(anArcOfCircle5);
  anEdge8 = BRepBuilderAPI_MakeEdge(aSegment3);
  anEdge9 = BRepBuilderAPI_MakeEdge(anArcOfCircle6);
  anEdge10 = BRepBuilderAPI_MakeEdge(anArcOfCircle7);
  anEdge11 = BRepBuilderAPI_MakeEdge(anArcOfCircle8);
  anEdge12 = BRepBuilderAPI_MakeEdge(aSegment4);
  TopoDS_Wire mWire =
      BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3, anEdge4);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge5);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge6);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge7);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge8);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge9);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge10);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge11);
  mWire = BRepBuilderAPI_MakeWire(mWire, anEdge12);
  Handle(AIS_Shape) sec3 = new AIS_Shape(mWire);
  // myAISContext->Display(sec3,false);
  // 合并
  BRepOffsetAPI_ThruSections tuopan(
      true,
      false); // 第1个参数true代表实体，否则代表shell；第2个参数true代表线性过渡，否则代表非线性过渡（曲线过渡）
  tuopan.AddWire(aWire);
  tuopan.AddWire(mWire);
  tuopan.AddWire(bWire);
  tuopan.Build();
  TopoDS_Shape S = tuopan.Shape();
  Handle(AIS_Shape) ais = new AIS_Shape(S);
  const Handle(AIS_InteractiveContext) &myAISContext =
      GetDocument()->GetAISContext();

  myAISContext->SetColor(ais, Quantity_NOC_MATRABLUE, false);
  myAISContext->SetMaterial(ais, Graphic3d_NOM_PLASTIC, false);
  myAISContext->Display(ais, false);
  const Handle(AIS_InteractiveObject) &anIO = ais;
  myAISContext->SetSelected(anIO, false);
  // Fit();
  // 顶帽
  P1.SetCoord(24, -15, 30);
  P2.SetCoord(39, 0, 30);
  P3.SetCoord(24, 15, 30);
  P1_1.SetCoord(-24, -15, 30);
  P2_1.SetCoord(-39, 0, 30);
  P3_1.SetCoord(-24, 15, 30);
  P1_2.SetCoord(2, -15, 30);
  P2_2.SetCoord(1.29, -14.71, 30);
  P3_2.SetCoord(1, -14, 30);
  P1_3.SetCoord(-2, -15, 30);
  P2_3.SetCoord(-1.29, -14.71, 30);
  P3_3.SetCoord(-1, -14, 30);
  P3_4.SetCoord(0, -13, 30);
  P1_5.SetCoord(2, 15, 30);
  P2_5.SetCoord(1.29, 14.71, 30);
  P3_5.SetCoord(1, 14, 30);
  P1_6.SetCoord(-2, 15, 30);
  P2_6.SetCoord(-1.29, 14.71, 30);
  P3_6.SetCoord(-1, 14, 30);
  P3_7.SetCoord(0, 13, 30);
  anArcOfCircle1 = GC_MakeArcOfCircle(P1_1, P2_1, P3_1);
  aSegment1 = GC_MakeSegment(P3_1, P1_6);
  anArcOfCircle2 = GC_MakeArcOfCircle(P1_6, P2_6, P3_6);
  anArcOfCircle3 = GC_MakeArcOfCircle(P3_6, P3_7, P3_5);
  anArcOfCircle4 = GC_MakeArcOfCircle(P3_5, P2_5, P1_5);
  aSegment2 = GC_MakeSegment(P1_5, P3);
  anArcOfCircle5 = GC_MakeArcOfCircle(P3, P2, P1);
  aSegment3 = GC_MakeSegment(P1, P1_2);
  anArcOfCircle6 = GC_MakeArcOfCircle(P1_2, P2_2, P3_2);
  anArcOfCircle7 = GC_MakeArcOfCircle(P3_2, P3_4, P3_3);
  anArcOfCircle8 = GC_MakeArcOfCircle(P3_3, P2_3, P1_3);
  aSegment4 = GC_MakeSegment(P1_3, P1_1);
  anEdge1 = BRepBuilderAPI_MakeEdge(anArcOfCircle1);
  anEdge2 = BRepBuilderAPI_MakeEdge(aSegment1);
  anEdge3 = BRepBuilderAPI_MakeEdge(anArcOfCircle2);
  anEdge4 = BRepBuilderAPI_MakeEdge(anArcOfCircle3);
  anEdge5 = BRepBuilderAPI_MakeEdge(anArcOfCircle4);
  anEdge6 = BRepBuilderAPI_MakeEdge(aSegment2);
  anEdge7 = BRepBuilderAPI_MakeEdge(anArcOfCircle5);
  anEdge8 = BRepBuilderAPI_MakeEdge(aSegment3);
  anEdge9 = BRepBuilderAPI_MakeEdge(anArcOfCircle6);
  anEdge10 = BRepBuilderAPI_MakeEdge(anArcOfCircle7);
  anEdge11 = BRepBuilderAPI_MakeEdge(anArcOfCircle8);
  anEdge12 = BRepBuilderAPI_MakeEdge(aSegment4);
  TopoDS_Wire dWire =
      BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3, anEdge4);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge5);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge6);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge7);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge8);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge9);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge10);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge11);
  dWire = BRepBuilderAPI_MakeWire(dWire, anEdge12);
  Handle(AIS_Shape) sec4 = new AIS_Shape(dWire);
  // myAISContext->Display(sec4,false);
  // 合并
  BRepOffsetAPI_ThruSections dingmao(
      true,
      false); // 第1个参数true代表实体，否则代表shell；第2个参数true代表线性过渡，否则代表非线性过渡（曲线过渡）
  dingmao.AddWire(bWire);
  dingmao.AddWire(dWire);
  dingmao.Build();
  TopoDS_Shape S1 = dingmao.Shape();
  Handle(AIS_Shape) ais1 = new AIS_Shape(S1);
  myAISContext->SetColor(ais1, Quantity_NOC_MATRABLUE, false);
  myAISContext->SetMaterial(ais1, Graphic3d_NOM_PLASTIC, false);
  myAISContext->Display(ais1, false);
  const Handle(AIS_InteractiveObject) &anIO1 = ais1;
  myAISContext->SetSelected(anIO1, false);
  // Fit();
  // 裁剪
  gp_Dir D(0, 1, 0);
  gp_Pnt p1, p2;
  BRepBuilderAPI_MakeWire MW;
  p1 = gp_Pnt(-7.5, -100, 30);
  p2 = gp_Pnt(-7.5, -100, 27);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  p1 = p2;
  p2 = gp_Pnt(-5.5, -100, 25);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  p1 = p2;
  p2 = gp_Pnt(5.5, -100, 25);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  p1 = p2;
  p2 = gp_Pnt(7.5, -100, 27);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  p1 = p2;
  p2 = gp_Pnt(7.5, -100, 30);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  p1 = p2;
  p2 = gp_Pnt(-7.5, -100, 30);
  MW.Add(BRepBuilderAPI_MakeEdge(p1, p2));
  TopoDS_Shape FP = BRepBuilderAPI_MakeFace(gp_Pln(gp::ZOX()), MW.Wire());
  BRepLib::BuildCurves3d(FP);
  TopoDS_Face F = BRepBuilderAPI_MakeFace(gp_Pln(gp::ZOX()));
  BRepFeat_MakePrism MKP(S, FP, F, D, 0, true);
  MKP.Perform(1000.);
  TopoDS_Shape res = MKP.Shape();
  BRepFeat_MakePrism MKP1(S1, FP, F, D, 0, true);
  MKP1.Perform(1000.);
  TopoDS_Shape res1 = MKP1.Shape();
  ais->Set(res);
  ais1->Set(res1);
  myAISContext->Redisplay(ais, false);
  myAISContext->Redisplay(ais1, false);
  myAISContext->SetSelected(anIO, false);
  myAISContext->SetSelected(anIO1, false);
  // 墩身
  P1.SetCoord(16, -16.67, -120);
  P2.SetCoord(32.67, 0, -120);
  P3.SetCoord(16, 16.67, -120);
  P1_1.SetCoord(-16, -16.67, -120);
  P2_1.SetCoord(-32.67, 0, -120);
  P3_1.SetCoord(-16, 16.67, -120);
  P1_2.SetCoord(2, -16.67, -120);
  P2_2.SetCoord(1.29, -16.37, -120);
  P3_2.SetCoord(1, -15.67, -120);
  P1_3.SetCoord(-2, -16.67, -120);
  P2_3.SetCoord(-1.29, -16.37, -120);
  P3_3.SetCoord(-1, -15.67, -120);
  P3_4.SetCoord(0, -14.67, -120);
  P1_5.SetCoord(2, 16.67, -120);
  P2_5.SetCoord(1.29, 16.37, -120);
  P3_5.SetCoord(1, 15.67, -120);
  P1_6.SetCoord(-2, 16.67, -120);
  P2_6.SetCoord(-1.29, 16.37, -120);
  P3_6.SetCoord(-1, 15.67, -120);
  P3_7.SetCoord(0, 14.67, -120);
  anArcOfCircle1 = GC_MakeArcOfCircle(P1_1, P2_1, P3_1);
  aSegment1 = GC_MakeSegment(P3_1, P1_6);
  anArcOfCircle2 = GC_MakeArcOfCircle(P1_6, P2_6, P3_6);
  anArcOfCircle3 = GC_MakeArcOfCircle(P3_6, P3_7, P3_5);
  anArcOfCircle4 = GC_MakeArcOfCircle(P3_5, P2_5, P1_5);
  aSegment2 = GC_MakeSegment(P1_5, P3);
  anArcOfCircle5 = GC_MakeArcOfCircle(P3, P2, P1);
  aSegment3 = GC_MakeSegment(P1, P1_2);
  anArcOfCircle6 = GC_MakeArcOfCircle(P1_2, P2_2, P3_2);
  anArcOfCircle7 = GC_MakeArcOfCircle(P3_2, P3_4, P3_3);
  anArcOfCircle8 = GC_MakeArcOfCircle(P3_3, P2_3, P1_3);
  aSegment4 = GC_MakeSegment(P1_3, P1_1);
  anEdge1 = BRepBuilderAPI_MakeEdge(anArcOfCircle1);
  anEdge2 = BRepBuilderAPI_MakeEdge(aSegment1);
  anEdge3 = BRepBuilderAPI_MakeEdge(anArcOfCircle2);
  anEdge4 = BRepBuilderAPI_MakeEdge(anArcOfCircle3);
  anEdge5 = BRepBuilderAPI_MakeEdge(anArcOfCircle4);
  anEdge6 = BRepBuilderAPI_MakeEdge(aSegment2);
  anEdge7 = BRepBuilderAPI_MakeEdge(anArcOfCircle5);
  anEdge8 = BRepBuilderAPI_MakeEdge(aSegment3);
  anEdge9 = BRepBuilderAPI_MakeEdge(anArcOfCircle6);
  anEdge10 = BRepBuilderAPI_MakeEdge(anArcOfCircle7);
  anEdge11 = BRepBuilderAPI_MakeEdge(anArcOfCircle8);
  anEdge12 = BRepBuilderAPI_MakeEdge(aSegment4);
  TopoDS_Wire sWire =
      BRepBuilderAPI_MakeWire(anEdge1, anEdge2, anEdge3, anEdge4);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge5);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge6);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge7);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge8);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge9);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge10);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge11);
  sWire = BRepBuilderAPI_MakeWire(sWire, anEdge12);
  Handle(AIS_Shape) sec5 = new AIS_Shape(sWire);
  myAISContext->Display(sec5, false);
  // 合并
  BRepOffsetAPI_ThruSections dunshen(
      true,
      false); // 第1个参数true代表实体，否则代表shell；第2个参数true代表线性过渡，否则代表非线性过渡（曲线过渡）
  dunshen.AddWire(sWire);
  dunshen.AddWire(aWire);
  dunshen.Build();
  TopoDS_Shape S2 = dunshen.Shape();
  Handle(AIS_Shape) ais2 = new AIS_Shape(S2);
  myAISContext->SetColor(ais2, Quantity_NOC_MATRABLUE, false);
  myAISContext->SetMaterial(ais2, Graphic3d_NOM_PLASTIC, false);
  myAISContext->Display(ais2, false);
  const Handle(AIS_InteractiveObject) &anIO2 = ais2;
  myAISContext->SetSelected(anIO2, false);
  // Fit();
  // 承台
  TopoDS_Shape S3 =
      BRepPrimAPI_MakeBox(gp_Pnt(-38.41, -22.22, -130), 76.82, 44.44, 10)
          .Shape();
  Handle(AIS_Shape) ais3 = new AIS_Shape(S3);
  myAISContext->SetColor(ais3, Quantity_NOC_GREEN, false);
  myAISContext->SetMaterial(ais3, Graphic3d_NOM_PLASTIC, false);
  myAISContext->Display(ais3, false);
  const Handle(AIS_InteractiveObject) &anIO3 = ais3;
  myAISContext->SetSelected(anIO3, false);
  // Fit();
  TopoDS_Shape S4 =
      BRepPrimAPI_MakeBox(gp_Pnt(-44.79, -29.53, -140), 89.59, 59.05, 10)
          .Shape();
  Handle(AIS_Shape) ais4 = new AIS_Shape(S4);
  myAISContext->SetColor(ais4, Quantity_NOC_GREEN, false);
  myAISContext->SetMaterial(ais4, Graphic3d_NOM_PLASTIC, false);
  myAISContext->Display(ais4, false);
  const Handle(AIS_InteractiveObject) &anIO4 = ais4;
  myAISContext->SetSelected(anIO4, false);
  // Fit();
  //******流线型托盘******

  m_occView->FitAll();
}
