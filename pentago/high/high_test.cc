// High level interface tests

#include "pentago/high/board.h"
#include "pentago/high/check.h"
#include "pentago/search/supertable.h"
#include "pentago/utility/char_view.h"
#include "pentago/utility/log.h"
#include "gtest/gtest.h"
namespace pentago {
namespace {

void board_test(const int slice, RawArray<const board_t> boards, RawArray<const uint64_t> wins) {
  Scope scope("board test");
  ASSERT_EQ(boards.size() * 8, wins.size());
  init_supertable(15);
  Array<Vector<super_t,2>> super_wins(wins.size() / 8, uninit);
  char_view(super_wins).copy(char_view(wins));
  slog("slice %d, samples %d", slice, boards.size());
  const auto counts = sample_check(*reader_block_cache({}, 1), boards, super_wins);
  slog("counts: loss %d, tie %d, win %d", counts[0], counts[1], counts[2]);
}

TEST(high, board_33) {
  const board_t boards[] = {2770068518349711244,2770030018707007453,2792799663139343586,2770023185559136596,2770031367330090210,946362534908277485,2770036521735166202,2156986100076914650,2362474322681010399,2770004605403015769,2770864247678506424,923533499761498528,945229144863549861,2226209450043060318,2846036856505043064,2154465015406988820,930880028643439236,925532119476742730,2747219245982165682,2109134049064989235,2770867679080943565,675020434118423253,2770050427979306820,2154455037858027882,2770885576634731260,239309724833949408,2770042203817641597,2154462309715611082,307694894062512714,741447876250707579,2770866747025267019,810412672450373585};
  const uint64_t wins[] = {2536128238408835890u,2536128238408835890u,2536128238408835890u,2536128238408835890u,14757395258967641292u,14757395258967641292u,14757395258967641292u,14757395258967641292u,562958543486976u,562958543486976u,562958543486976u,562958543486976u,17289582787572068336u,17289582787572068336u,17289582787572068336u,17289582787572068336u,280379743272960u,280379743272960u,280379743272960u,280379743272960u,1095233372415u,1095233372415u,1095233372415u,1095233372415u,18446462599019167744u,18446462599019167744u,18446462599019167744u,18446462599019167744u,262709978263278u,262709978263278u,262709978263278u,262709978263278u,18374686483949879040u,18374686483949879040u,18374686483949879040u,18374686483949879040u,0u,0u,0u,0u,16842623u,280920942706688u,16842623u,280920942706688u,18446744073692708864u,18446462603011031039u,18446744073692708864u,18446462603011031039u,1229782938247303441u,1229782938247303441u,1229782938247303441u,1229782938247303441u,0u,0u,0u,0u,8224261361801232384u,8224173400871039522u,8224261361801232384u,125490927137314u,65535u,243941257510912u,65535u,8608349216464240640u,13767433141282127743u,13776440340538965887u,12836032443129708415u,13767379883687644943u,67553994426286080u,58546795169447936u,998954692578705408u,67678170520776944u,1808494317973082112u,1808494317973082112u,1808494317973082112u,1808494317973082112u,7378697629483820646u,7378697629483820646u,7378697629483820646u,7378697629483820646u,4611949243809530112u,17288831082386817280u,17289019241961357568u,17288831082386817280u,13834776580305514222u,206411833339630u,3221221102u,206411833339630u,1099528405248u,72058693566333184u,1099528405248u,72058693566333184u,18446462598732840960u,0u,18446462598732840960u,0u,18446481367739858944u,17294086455937798144u,17294086455919906816u,18446462603027742720u,65535u,1152657617771696127u,1152657617789587455u,17587891142655u,17293822573129297920u,4293918720u,983040u,1085086035472224015u,1152657617789587455u,18446744069414649855u,18442521884633399280u,17361641481138401520u,278931492878763u,13334030053253363979u,278931492878763u,13334030053253363979u,18446462598732840960u,67553994410557680u,18446462598732840960u,67553994410557680u,0u,842137600u,842137600u,0u,18446744073709551615u,18446744072850571263u,18446744072850571263u,18446744073709551615u,2533876404234887978u,2533876404234887978u,2533876404234887978u,38663885562666u,0u,0u,0u,18446462598732840960u,16369037670802391296u,9297681434517569792u,9297823305877291264u,9297931204045701376u,112588272697344u,7378697627765833728u,7378697627765833728u,7378585039493136384u,0u,0u,0u,0u,281470681808895u,281470681808895u,281470681808895u,281470681808895u,286265616u,286265616u,6582573805641685850u,100443637813520u,18446744073423220462u,18446744073423220462u,281474690318336u,18446462598732902126u,4629771061704314060u,8088553168516218847u,8088553168516218847u,4629771061704327135u,13527612320720302899u,10068795029536309248u,10068795029536309248u,13527612320720289792u,17294090759477194752u,62218u,0u,63051356855861248u,1152640029898510335u,18446744073709486080u,18446744073709551615u,18379189048491114255u,9277555973092507648u,9277555973092507648u,9277555973092507648u,9277555973092507648u,3689348813882929971u,3689348813882929971u,3689348813882929971u,3689348813882929971,17298044697470374319u,18378916304620687791u,17298044697470374319u,18378916304620687791u,1148699375661219840u,67818972417884160u,1148699375661219840u,67818972417884160u,4919131752989261823u,4919131756138856447u,4919131756138808388u,4919131756138856447u,13527612320720289792u,13527612317570695168u,13527612317570743227u,13527612317570695168u,17942200792136581263u,17942200794426966159u,71776183485726720u,17365598752588955663u,33554432u,2201021906688u,18374966859431673855u,4278255360u,2459528347070234624u,2459528347070234624u,2459528347070234624u,2459528347070234624u,15987215726639120384u,15987215726639120384u,15987215726639120384u,15987215726639120384u,2459565876494606882u,2459584641493054259u,2459584641493054259u,2459565876494606882u,15987178197214944733u,15987159432216497356u,15987159432216497356u,15987178197214944733u,2290480879370240u,2535002321321861888u,2534964774144320256u,74348074917298432u,8608349212742057983u,0u,112592281272320u,7378585039493197550u,281474976645120u,281471540723712u,153417090793472u,281474976645120u,7378585039493201919u,7378585042928336880u,7378695432178565119u,7378585039493201919u,6196953084684813380u,6196953084684861439u,4919131753561915391u,6196953084684861439u,2308549563u,2308505600u,13527612320147636224u,2308505600u,18694913266944u,1513228169709753600u,1513228169709753600u,1513228169709753600u,16910711684942064302u,16910711684942064302u,16910711684942064302u,16910711684942064302u};
  board_test(33, asarray(boards), asarray(wins));
}

TEST(high, board_34) {
  const board_t boards[] = {2770041146411456919,2770038673513783845,2770050553393319851,2770013546935307490,923292882872380976,900481122287364025,946112525136438820,2770041731530493931,2802982536206887515,1128478618662948066,2748082826466439593,2770038711881834808,2770041700430007522,2792847903933075378,948614450817011908,2771194894743119477,2770321114698889945,2978042358059443945,2093653277386296546,2770320139751928506,2155296512342246484,2770309947511413149,2838721586901232013,925826497389862464,2230435388789368470,2792848973808150789,2770884996525730486,315294593736126568,2086053668358079332,2770008792978574562,3000569749847548417,2770853913281178267};
  const uint64_t wins[] = {52777364500480u,52777364500480u,52777364500480u,52777364500480u,18446462598732840960u,18446462598732840960u,18446462598732840960u,18446462598732840960u,0u,0u,0u,0u,0u,0u,0u,0u,15986934256530030592u,15986934256530030592u,15986934256530030592u,15986934256530030592u,2459659699482556279u,2459659699482556279u,2459659699482556279u,2459659699482556279u,1080880403494993920u,1080880403494993920u,1080880403494993920u,1080880403494993920u,61695u,61695u,61695u,61695u,7455410111385593718u,7455410111385593718u,7455410111385593718u,7455410111385593718u,9838113385990848512u,9838113385990848512u,10991333962323918848u,9838113385990848512u,37529424232448u,37529424232448u,37529424232448u,37529424232448u,7378585039493136384u,7378585039493136384u,7378660098341601280u,7378585039493201919u,17294051271547867136u,17294051271547867136u,17294051271547867136u,17294051271547867136u,1152657617789587455u,1152657617789587455u,1148435428713435120u,1148435428713435120u,7378697629483820646u,7378697629483820646u,7378697629483820646u,7378697629483820646u,0u,0u,0u,0u,13401324889025745403u,18302626686576885247u,18302626686576885247u,651615718060523787u,144117387132666368u,144117387132666368u,144117387099111936u,17505758868271067888u,17294086455919964160u,17294086455919964160u,17294086455919964160u,17294086455919964160u,0u,0u,0u,0u,1229782938247303441u,1229782938247303441u,1229782938247303441u,1229782938247303441u,187647121162240u,0u,61166u,0u,72621652109820162u,72621652109820162u,72621652109820162u,72621652109820162u,0u,0u,0u,0u,1080880403494997760u,1080880403494997760u,1080880403494997760u,1080880403494997760u,0u,0u,0u,0u,5787219346163324245u,5787219346163324245u,5787219346163324245u,5787219346163324245u,12659337077561753600u,12659337077561753600u,12659337077561753600u,12659337077561753600u,6922154808067443724u,6917651208389741580u,6922154808067443724u,6922154808067443724u,1148136430017445888u,11524589265642061824u,11520367140991448048u,11524429973894987776u,823989698560u,18140494625479973273u,18140494625479973273u,53762540900778137u,18446556332099108864u,306249447656916036u,306249447585611776u,4918005835902239812u,7608384715226507670u,7608384715226507670u,7608384715226507670u,7608384715226507670u,0u,38505u,38505u,0u,32637851408790515u,32637851408790515u,8319261165770503155u,32637851408790387u,18269977788402633740u,18269977788402633740u,10127469662116776972u,18269977788402633740u,1095233372160u,1095233372160u,1095233372160u,1095233372160u,18446462598732844800u,1152656522556215040u,16492926074880u,18442256967008259840u,2533876404234879240u,2533876404234879240u,2533876404234879240u,2533876404234879240u,15911460269411270656u,14753792379265806048u,15911217483499962368u,14753792379265744896u,5764695485306654720u,5764695485306654720u,5764695485306654720u,5764695485306654720u,12682048588402896895u,0u,12682048588402896895u,0u,9223512780785680385u,9223512780785680385u,9223512780785680385u,9223512780785680385u,140728898453502u,9223231290776485888u,140728898420736u,140728898453502u,12371013993126341550u,12371013993126341550u,12371013993126341550u,12371013993126341550u,92706869084160u,21585u,6075730080583188480u,21585u,1230045648225566719u,1230045643930599424u,1230045648225566719u,1230045648225566719u,9838113385990848512u,9838113388567789568u,9838113385990848512u,9838113385990848512u,18446518893728235519u,18446518893728235519u,18446518893728235519u,18446518893728235519u,225179981316096u,225179981316096u,225179981316096u,225179981316096u,17293945718699913216u,17293945718699913216u,17293945718699913216u,17293945718699913216u,1152798355009572863u,1152798355009572863u,1152798355009572863u,1152798355009572863u,1253708676345500006u,1253708676345500006u,1253708676345500006u,1253708676345500006u,262340897406976u,17152502378239361024u,262340897406976u,261722422116352u,9007336695791648u,9007336695791648u,9007336695791648u,9007336695791648u,18437455399478099968u,18437455399478165471u,281333242789888u,18437736737013759967u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,17361376567555584240u,17361376567555584240u,17361376567555584240u,17361376567555584240u,18302628884023083007u,10955456453541468569u,18302628884023083007u,10955456453541468569u,0u,7378697629483820646u,0u,7378697629483820646u,7071322264739930658u,7071322264739930658u,7071322264739930658u,7071322264739930658u,0u,11375248236456287709u,0u,40413u};
  board_test(34, asarray(boards), asarray(wins));
}

TEST(high, board_35) {
  const board_t boards[] = {2770019925981538179,2770042195386967854,2770323566422730384,2778483158587085068,2793685590919486989,924129152089597800,2747238401536305845,2770320551739463984,2770013861057737266,925814677501848135,2770037990613979305,2770037169701932258,2770031255531375842,1538878880271510170,2770039652617695822,2770036856742619821,2770042203971587397,2178099867909239292,2773419516290410208,923280551968197858,2793121416327079808,2770031655089550115,2770020119255069154,1561665490393376651,2792841560536915794,2770014011381589737,2792811942883242736,2245649648803466466,2770037642004810978,2711772326407514067,2223695799075023191,2792841642444072643};
  const uint64_t wins[] = {3689348814741910323u,3689348814741910323u,3689348814741910323u,3689348814741910323u,1145324612u,1145324612u,1145324612u,1145324612u,2863267840u,2863267840u,2863267840u,2863267840u,18446744070846218240u,18446744070846218240u,18446744070846218240u,18446744070846218240u,2576478208u,12731869663199342592u,11574426966779076608u,12731869663199358976u,18446744070559891456u,4919131752989213764u,6148914690950190421u,4919131752989196288u,1407397718751637u,415155834157461u,133680857119125u,1342468100u,18442521880339415040u,18442240478376099840u,18446462603027742720u,18446744069414584320u,0u,150117696990958u,150117696990958u,150117696990958u,18446593956012556288u,18446593956012556288u,13527462203023360000u,18446593956012556288u,13527405908887059387u,13527405908887059387u,572688520u,13527405908887059387u,4919150517701329988u,4919131752989196288u,18446744073136863095u,75059993789508u,0u,0u,0u,0u,0u,0u,0u,0u,9223550157741250047u,14144962000348398668u,9225905307352978943u,14144962000348403199u,6761413585405018112u,1842253723000191923u,2149624397568212992u,1842253723000176640u,1360u,1360u,1360u,1360u,18446744073709486080u,18446744073709486080u,18446744073709486080u,18446744073709486080u,3691675708902438673u,16607536724670225u,3691675674542669824u,3691675708902408192u,4899916394579099648u,4919206810692354048u,4919206810692419583u,4919056692995489791u,2459565876494598144u,2459565876494598144u,2459565876494598144u,2459565876494598144u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,4629771064853872704u,286326784u,286326784u,4629771064853872704u,0u,0u,0u,0u,649091200623577346u,649091200623577346u,649091200623577346u,649091200623577346u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,14757170080131610487u,14757170080131610487u,14757170080131610487u,14757170080131610487u,3689348814741897216u,3689348814741897216u,3689348814741897216u,3689348814741897216u,67371008u,67436543u,67371008u,67436543u,18446667910801832634u,18446667910801784832u,18446667910801832634u,18446667910801784832u,858993459u,855651072u,858980352u,858980352u,8608630683423735808u,18446744072850505728u,18446744069414649855u,7378866511320213094u,17294086455919902720u,17294086455919902720u,17293822573129236480u,17294086455919902720u,0u,0u,281470681743360u,65535u,550395609011u,9347995730944u,3458826638374433792u,3458826638209482675u,18158359181174571008u,18158359177519496191u,864413463114419199u,864413463145676800u,9223512776550088704u,9223512776550088704u,9223512776550088704u,9223512776550088704u,9219009105996316656u,9219009105996316656u,9219009105996316656u,9219009105996316656u,12297641735351872170u,12297641735351872170u,12297641735351872170u,12297641735351872170u,0u,0u,0u,0u,7378697629483794432u,7378697629483820646u,0u,7378697629483794432u,13107u,0u,7378678863053717504u,13107u,8014824041049243648u,8014824041049243648u,8014824041049243648u,8014824041049243648u,17523733954560u,17523733954560u,17523733954560u,17523733954560u,35184372097024u,35184372097024u,35184372097024u,35184372097024u,6148820870002368511u,6148820870002368511u,6148820870002368511u,6148820870002368511u,72620543991349506u,72620543991349506u,72620543991349506u,72620543991349506u,281474976645120u,281474976645120u,281474976645120u,281474976645120u,263886817259520u,263886817259520u,4026593280u,263886817198080u,1152640029630140400u,17587891077120u,280375465082880u,18374686479671688960u,0u,0u,0u,0u,0u,0u,0u,0u,2459565876494606882u,572662306u,37529996894754u,572662306u,0u,8608480565726806016u,8608349212742049245u,8608480565726806016u,1225466917020569857u,1225466917020569861u,1225466917020569857u,1225541700991127809u,7378660102350237422u,316452936679817216u,7378660102350237422u,7378585043501772526u,4290510779u,4290510779u,4290510779u,4290510779u,18446744069414584320u,18446744069414584320u,18446744069414584320u,18446744069414584320u};
  board_test(35, asarray(boards), asarray(wins));
}

}  // namespace
}  // namespace pentago

/*
def bootstrap(slice,n,verbose=1):
  samples = load('../../data/edison/project/all/sparse-%d.npy'%slice)
  random.seed(81311)
  random.shuffle(samples)
  samples = samples[:n]
  if verbose:
    print('boards[%d] = asarray(%s,dtype=uint64)'%(slice,compact_str(samples[:,0])))
    print('wins[%d] = asarray(%s,dtype=uint64).reshape(-1,2,4)'%(slice,compact_str(samples[:,1:].ravel())))
  return samples[:,0].copy(),samples[:,1:].reshape(-1,2,4).copy()

if __name__=='__main__':
  data = None
  if '--bootstrap' in sys.argv:
    for slice in 33,34,35:
      bootstrap(slice,n=32)
  elif len(sys.argv)==2:
    for slice in 33,34,35:
      board_test(slice,*bootstrap(slice,n=int(sys.argv[1]),verbose=0))
  else:
    test_board_33()
    test_board_34()
    test_board_35()
*/
