function PlotPosteriorDistributions()
% stephane.adjemian@ens.fr [09-09-2005]
global estim_params_ M_ options_ bayestopt_ oo_

DirectoryName = CheckPath('Plots\Densities');

TeX   	= options_.TeX;
nblck 	= options_.mh_nblck;
nvx   	= estim_params_.nvx;
nvn   	= estim_params_.nvn;
ncx   	= estim_params_.ncx;
ncn   	= estim_params_.ncn;
np    	= estim_params_.np ;
npar   	= nvx+nvn+ncx+ncn+np;

MaxNumberOfPlotPerFigure = 9;% The square root must be an integer!
nn = sqrt(MaxNumberOfPlotPerFigure);

figurename = 'Priors and posteriors';

if TeX    
  fidTeX = fopen([DirectoryName '\' M_.fname '_PriorsAndPosteriors.TeX'],'w');
  fprintf(fidTeX,'%% TeX eps-loader file generated by PlotPosteriorDistributions.m (Dynare).\n');
  fprintf(fidTeX,['%% ' datestr(now,0) '\n']);
  fprintf(fidTeX,' \n');
end

figunumber = 0;
subplotnum = 0;
for i=1:npar
  subplotnum = subplotnum+1;
  if subplotnum == 1
    figunumber = figunumber+1;
    hfig = figure('Name',figurename);
  end  
  if subplotnum == 1
    if TeX
      TeXNAMES = [];
    end
    NAMES = [];
  end
  [nam,texnam] = get_the_name(i,TeX);
  NAMES = strvcat(NAMES,nam);
  if TeX
    TeXNAMES = strvcat(TeXNAMES,texnam);
  end
  [x2,f2,abscissa,dens,binf2,bsup2] = draw_prior_density(i);
  top2 = max(f2); 
  if i <= nvx
    name = deblank(M_.exo_names(estim_params_.var_exo(i,1),:));    
    eval(['x1 = oo_.posterior_density.shocks_std.' name '(:,1);'])
    eval(['f1 = oo_.posterior_density.shocks_std.' name '(:,2);'])    
    eval(['pmode = oo_.posterior_mode.shocks_std.' name ';'])  
  elseif i <= nvx+nvn
    name = deblank(options_.varobs(estim_params_.var_endo(i-nvx,1),:));
    eval(['x1 = oo_.posterior_density.measurement_errors_std.' name '(:,1);'])
    eval(['f1 = oo_.posterior_density.measurement_errors_std.' name '(:,2);'])    
    eval(['pmode = oo_.posterior_mode.measurement_errors_std.' name ';'])  
  elseif i <= nvx+nvn+ncx
    j = i - (nvx+nvn)
    k1 = estim_params_.corrx(j,1);
    k2 = estim_params_.corrx(j,2);
    name = [deblank(M_.exo_names(k1,:)) '_' deblank(M_.exo_names(k2,:))];  
    eval(['x1 = oo_.posterior_density.shocks_corr.' name '(:,1);'])
    eval(['f1 = oo_.posterior_density.shocks_corr.' name '(:,2);'])    
    eval(['pmode = oo_.posterior_mode.shocks_corr.' name ';'])  
  elseif i <= nvx+nvn+ncx+ncn
    j = i - (nvx+nvn+ncx);
    k1 = estim_params_.corrn(j,1);
    k2 = estim_params_.corrn(j,2);
    name = [deblank(M_.endo_names(k1,:)) '_' deblank(M_.endo_names(k2,:))];
    eval(['x1 = oo_.posterior_density.measurement_errors_corr.' name '(:,1);'])
    eval(['f1 = oo_.posterior_density.measurement_errors_corr.' name '(:,2);'])
    eval(['pmode = oo_.posterior_mode.measurement_errors_corr.' name ';'])
  else
    j = i - (nvx+nvn+ncx+ncn);
    name = deblank(M_.param_names(estim_params_.param_vals(j,1),:));
    eval(['x1 = oo_.posterior_density.' name '(:,1);'])
    eval(['f1 = oo_.posterior_density.' name '(:,2);'])
    eval(['pmode = oo_.posterior_mode.parameters.' name ';'])
  end
  top1 = max(f1);
  top0 = max([top1;top2]);
  binf1 = x1(1);
  bsup1 = x1(end);
  borneinf = min(binf1,binf2);
  bornesup = max(bsup1,bsup2);
  subplot(nn,nn,subplotnum)
  hh = plot(x2,f2,'-k','linewidth',2);
  set(hh,'color',[0.7 0.7 0.7]);
  hold on;
  plot(x1,f1,'-k','linewidth',2);
  plot( [pmode pmode], [0.0 1.1*top0], '--g', 'linewidth', 2);
  box on;
  axis([borneinf bornesup 0 1.1*top0]);
  title(nam,'Interpreter','none');
  hold off;
  drawnow
  if subplotnum == MaxNumberOfPlotPerFigure | i == npar;
    eval(['print -depsc2 ' DirectoryName '\' M_.fname '_PriorsAndPosteriors' int2str(figunumber)]);
    eval(['print -dpdf ' DirectoryName '\' M_.fname '_PriorsAndPosteriors' int2str(figunumber)]);
    saveas(hfig,[DirectoryName '\' M_.fname '_PriorsAndPosteriors' int2str(figunumber) '.fig']);
    if TeX
      fprintf(fidTeX,'\\begin{figure}[H]\n');
      for j = 1:size(NAMES,1)
	fprintf(fidTeX,'\\psfrag{%s}[1][][0.5][0]{%s}\n',deblank(NAMES(j,:)),deblank(TeXNAMES(j,:)));
      end    
      fprintf(fidTeX,'\\centering\n');
      fprintf(fidTeX,'\\includegraphics[scale=0.5]{%s_PriorsAndPosteriors%s}\n',M_.fname,int2str(figunumber));
      fprintf(fidTeX,'\\caption{Priors and posteriors.}');
      fprintf(fidTeX,'\\label{Fig:PriorsAndPosteriors:%s}\n',int2str(figunumber));
      fprintf(fidTeX,'\\end{figure}\n');
      fprintf(fidTeX,' \n');
      if i == npar
	fprintf(fidTeX,'%% End of TeX file.\n');
	fclose(fidTeX);
      end
    end
    if options_.nograph, close(hfig), end
    subplotnum = 0;
  end
end