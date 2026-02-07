let taskId = null;
let task = null;
let literatures = [];
let selectedIds = new Set();
let refreshInterval = null;
let currentView = 'list';
let currentLitIndex = 0;
let showEnglish = false;
let fontSize = 'medium';

document.addEventListener('DOMContentLoaded', function() {
    var params = new URLSearchParams(window.location.search);
    taskId = params.get('id');
    if (!taskId) {
        showToast('任务ID无效', 'error');
        setTimeout(function() { window.location.href = '/queue.html'; }, 1500);
        return;
    }
    loadTask();
    setupEventListeners();
    setupKeyboardNavigation();
    refreshInterval = setInterval(loadTask, 2000);
});

window.addEventListener('beforeunload', function() {
    if (refreshInterval) clearInterval(refreshInterval);
});

function setupEventListeners() {
    document.getElementById('listViewBtn').addEventListener('click', function() { setView('list'); });
    document.getElementById('detailViewBtn').addEventListener('click', function() { setView('detail'); });
    document.getElementById('prevLitBtn').addEventListener('click', prevLiterature);
    document.getElementById('nextLitBtn').addEventListener('click', nextLiterature);
    document.getElementById('exportBtn').addEventListener('click', exportSelected);
    document.getElementById('selectAllBtn').addEventListener('click', selectAll);
    document.getElementById('clearSelectionBtn').addEventListener('click', clearSelection);
    document.getElementById('pauseBtn').addEventListener('click', pauseTask);
    document.getElementById('resumeBtn').addEventListener('click', resumeTask);
    document.getElementById('retryFailedBtn').addEventListener('click', retryFailed);
    document.getElementById('retryWithModelBtn').addEventListener('click', retryWithModel);
    document.getElementById('resetTaskBtn').addEventListener('click', resetTask);
    document.getElementById('showEnglish').addEventListener('change', function() {
        showEnglish = this.checked;
        renderLiteratures();
    });
    document.getElementById('fontSize').addEventListener('change', function() {
        fontSize = this.value;
        renderLiteratures();
    });
    
    var litIndexInput = document.getElementById('litIndexInput');
    litIndexInput.addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            jumpToLiterature();
        }
    });
    litIndexInput.addEventListener('change', jumpToLiterature);
}

function setupKeyboardNavigation() {
    document.addEventListener('keydown', function(e) {
        if (currentView !== 'detail') return;
        if (document.activeElement.tagName === 'INPUT' || document.activeElement.tagName === 'SELECT') return;
        
        if (e.key === 'PageUp') {
            e.preventDefault();
            prevLiterature();
        } else if (e.key === 'PageDown') {
            e.preventDefault();
            nextLiterature();
        } else if (e.key === 'Enter') {
            e.preventDefault();
            if (literatures.length > 0) {
                toggleSelect(literatures[currentLitIndex].recordNumber);
            }
        }
    });
}

function jumpToLiterature() {
    var input = document.getElementById('litIndexInput');
    var index = parseInt(input.value);
    if (isNaN(index) || index < 1 || index > literatures.length) {
        showToast('请输入有效的文献编号 (1-' + literatures.length + ')', 'warning');
        return;
    }
    currentLitIndex = index - 1;
    renderDetailView();
}

function setView(view) {
    currentView = view;
    var listBtn = document.getElementById('listViewBtn');
    var detailBtn = document.getElementById('detailViewBtn');
    var detailNav = document.getElementById('detailNav');
    var fontSizeControl = document.getElementById('fontSizeControl');
    var showEnglishControl = document.getElementById('showEnglishControl');
    
    if (view === 'list') {
        listBtn.classList.add('bg-white', 'text-blue-600', 'shadow-sm');
        listBtn.classList.remove('text-slate-600');
        detailBtn.classList.remove('bg-white', 'text-blue-600', 'shadow-sm');
        detailBtn.classList.add('text-slate-600');
        detailNav.classList.add('hidden');
        // 列表视图隐藏字体大小和显示英文选项
        if (fontSizeControl) fontSizeControl.classList.add('hidden');
        if (showEnglishControl) showEnglishControl.classList.add('hidden');
    } else {
        detailBtn.classList.add('bg-white', 'text-blue-600', 'shadow-sm');
        detailBtn.classList.remove('text-slate-600');
        listBtn.classList.remove('bg-white', 'text-blue-600', 'shadow-sm');
        listBtn.classList.add('text-slate-600');
        detailNav.classList.remove('hidden');
        // 详情视图显示字体大小和显示英文选项
        if (fontSizeControl) fontSizeControl.classList.remove('hidden');
        if (showEnglishControl) showEnglishControl.classList.remove('hidden');
    }
    renderLiteratures();
}

async function loadTask() {
    try {
        task = await apiCall('GET', '/api/tasks/' + taskId);
        literatures = await apiCall('GET', '/api/tasks/' + taskId + '/literatures');
        renderTask();
        renderLiteratures();
        updateSelection();
    } catch (error) {
        if (refreshInterval) clearInterval(refreshInterval);
        document.getElementById('literatureContent').innerHTML = '<div class="p-8 text-center text-red-500">加载失败: ' + error.message + '</div>';
    }
}

function renderTask() {
    var status = getTaskStatusDisplay(task.status);
    var progress = task.totalCount > 0 ? (task.completedCount / task.totalCount * 100) : 0;
    
    document.getElementById('taskTitle').textContent = task.fileName;
    document.getElementById('taskId').textContent = task.taskId;
    document.getElementById('taskStatus').textContent = status.text;
    document.getElementById('taskStatus').className = 'px-3 py-1 text-sm font-medium rounded-full ' + status.bg + ' ' + status.color;
    document.getElementById('progressText').textContent = task.completedCount + ' / ' + task.totalCount + ' (' + progress.toFixed(1) + '%)';
    document.getElementById('progressBar').style.width = progress + '%';
    
    var failedInfo = document.getElementById('failedInfo');
    if (task.failedCount > 0) {
        failedInfo.textContent = task.failedCount + ' 篇翻译失败';
        failedInfo.classList.remove('hidden');
    } else {
        failedInfo.classList.add('hidden');
    }
    
    var pauseBtn = document.getElementById('pauseBtn');
    var resumeBtn = document.getElementById('resumeBtn');
    var retryFailedBtn = document.getElementById('retryFailedBtn');
    var retryWithModelBtn = document.getElementById('retryWithModelBtn');
    var resetTaskBtn = document.getElementById('resetTaskBtn');
    
    pauseBtn.classList.add('hidden');
    resumeBtn.classList.add('hidden');
    retryFailedBtn.classList.add('hidden');
    retryWithModelBtn.classList.add('hidden');
    resetTaskBtn.classList.add('hidden');
    document.getElementById('retryFailedCount').textContent = '';
    
    if (task.status === 2) {
        pauseBtn.classList.remove('hidden');
    } else if (task.status === 3) {
        resumeBtn.classList.remove('hidden');
    }
    
    // 任务完成、失败或暂停时显示重新翻译按钮
    if (task.status === 3 || task.status === 4 || task.status === 5) {
        resetTaskBtn.classList.remove('hidden');
        if (task.failedCount > 0) {
            retryFailedBtn.classList.remove('hidden');
            retryWithModelBtn.classList.remove('hidden');
            document.getElementById('retryFailedCount').textContent = task.failedCount + ' 篇失败';
        }
    }
    
    document.getElementById('viewOriginal').href = '/api/tasks/' + taskId + '/original.html';
    var viewTranslated = document.getElementById('viewTranslated');
    if (task.status === 4) {
        viewTranslated.href = '/api/tasks/' + taskId + '/translated.html';
        viewTranslated.classList.remove('hidden');
    }
}

function renderLiteratures() {
    var content = document.getElementById('literatureContent');
    document.getElementById('litCount').textContent = '(' + literatures.length + '篇)';
    
    if (literatures.length === 0) {
        content.innerHTML = '<div class="p-8 text-center text-slate-500">暂无文献</div>';
        return;
    }
    
    if (currentView === 'list') {
        renderListView();
    } else {
        renderDetailView();
    }
}

function renderListView() {
    var content = document.getElementById('literatureContent');
    var html = '<div class="divide-y divide-gray-100">';
    
    for (var i = 0; i < literatures.length; i++) {
        var lit = literatures[i];
        var isSelected = selectedIds.has(lit.recordNumber);
        var statusClass = getStatusClass(lit.status);
        var statusText = getStatusText(lit.status);
        var title = lit.translatedTitle || lit.originalTitle || '(无标题)';
        
        html += '<div class="px-4 py-2 hover:bg-slate-50 transition-colors flex items-center space-x-3 cursor-pointer ' + (isSelected ? 'bg-blue-50' : '') + '" onclick="viewLiterature(' + i + ')">';
        html += '<span class="text-xs text-slate-400 w-8">#' + lit.recordNumber + '</span>';
        html += '<span class="px-1.5 py-0.5 text-xs rounded ' + statusClass + '">' + statusText + '</span>';
        html += '<p class="flex-1 text-sm text-slate-900 truncate">' + title + '</p>';
        if (lit.translatedByModel) {
            html += '<span class="px-1.5 py-0.5 text-xs rounded bg-green-100 text-green-700 flex-shrink-0 mr-1" title="' + lit.translatedByModel + '">' + lit.translatedByModel + '</span>';
        }
        html += '<button onclick="event.stopPropagation(); toggleSelect(' + lit.recordNumber + ')" class="p-1.5 rounded hover:bg-blue-100 transition-colors cursor-pointer ' + (isSelected ? 'text-blue-600' : 'text-slate-400') + '" title="' + (isSelected ? '取消选择' : '选择导出') + '">';
        html += '<svg class="w-4 h-4" fill="' + (isSelected ? 'currentColor' : 'none') + '" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>';
        html += '</button>';
        html += '</div>';
    }
    
    html += '</div>';
    content.innerHTML = html;
}

function renderDetailView() {
    var content = document.getElementById('literatureContent');
    
    if (currentLitIndex >= literatures.length) {
        currentLitIndex = 0;
    }
    
    var lit = literatures[currentLitIndex];
    var isSelected = selectedIds.has(lit.recordNumber);
    var statusClass = getStatusClass(lit.status);
    var statusText = getStatusText(lit.status);
    
    // 字体大小类
    var titleSize = fontSize === 'small' ? 'text-base' : fontSize === 'large' ? 'text-2xl' : 'text-lg';
    var textSize = fontSize === 'small' ? 'text-xs' : fontSize === 'large' ? 'text-base' : 'text-sm';
    
    var html = '<div class="p-6">';
    
    // 头部信息
    html += '<div class="flex items-start justify-between mb-4">';
    html += '<div class="flex items-center space-x-2">';
    html += '<span class="text-sm text-slate-400">#' + lit.recordNumber + '</span>';
    html += '<span class="px-2 py-0.5 text-xs rounded-full ' + statusClass + '">' + statusText + '</span>';
    // 显示源文件信息（如果有）
    if (lit.sourceFileName) {
        html += '<span class="px-2 py-0.5 text-xs rounded-full bg-slate-100 text-slate-600" title="来源: ' + lit.sourceFileName + ' #' + lit.indexInFile + '">';
        html += '<svg class="w-3 h-3 inline mr-1" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z"></path></svg>';
        html += '文件' + lit.sourceFileIndex + ' #' + lit.indexInFile;
        html += '</span>';
    }
    // 显示翻译模型
    if (lit.translatedByModel) {
        html += '<span class="px-2 py-0.5 text-xs rounded-full bg-green-100 text-green-700" title="翻译模型: ' + lit.translatedByModel + '">';
        html += '<svg class="w-3 h-3 inline mr-1" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z"></path></svg>';
        html += lit.translatedByModel;
        html += '</span>';
    }
    html += '</div>';
    html += '<button onclick="toggleSelect(' + lit.recordNumber + ')" class="px-4 py-2 rounded-lg transition-colors cursor-pointer flex items-center space-x-2 ' + (isSelected ? 'bg-blue-600 text-white' : 'bg-blue-100 text-blue-600 hover:bg-blue-200') + '">';
    html += '<svg class="w-4 h-4" fill="' + (isSelected ? 'currentColor' : 'none') + '" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg>';
    html += '<span>' + (isSelected ? '已选择' : '选择 (Enter)') + '</span>';
    html += '</button>';
    html += '</div>';
    
    // 翻译标题 (中文)
    if (lit.translatedTitle) {
        html += '<div class="mb-4">';
        html += '<p class="text-xs text-slate-500 mb-1">标题</p>';
        html += '<p class="' + titleSize + ' font-medium text-blue-600">' + lit.translatedTitle + '</p>';
        html += '</div>';
    }
    
    // 翻译摘要 (中文)
    if (lit.translatedAbstract) {
        html += '<div class="mb-4">';
        html += '<p class="text-xs text-slate-500 mb-1">摘要</p>';
        html += '<p class="' + textSize + ' text-slate-700 leading-relaxed">' + lit.translatedAbstract + '</p>';
        html += '</div>';
    }
    
    // 英文内容 (根据复选框显示)
    if (showEnglish) {
        if (lit.originalTitle) {
            html += '<div class="mb-4 p-3 bg-slate-50 rounded-lg">';
            html += '<p class="text-xs text-slate-500 mb-1">英文标题</p>';
            html += '<p class="' + textSize + ' text-slate-600">' + lit.originalTitle + '</p>';
            html += '</div>';
        }
        
        if (lit.originalAbstract) {
            html += '<div class="mb-4 p-3 bg-slate-50 rounded-lg">';
            html += '<p class="text-xs text-slate-500 mb-1">英文摘要</p>';
            html += '<p class="' + textSize + ' text-slate-600 leading-relaxed">' + lit.originalAbstract + '</p>';
            html += '</div>';
        }
    }
    
    // 作者
    if (lit.authors) {
        html += '<div class="mb-4">';
        html += '<p class="text-xs text-slate-500 mb-1">作者</p>';
        html += '<p class="' + textSize + ' text-slate-700">' + lit.authors + '</p>';
        html += '</div>';
    }
    
    // DOI
    if (lit.doi) {
        html += '<div class="mb-4">';
        html += '<p class="text-xs text-slate-500 mb-1">DOI</p>';
        html += '<p class="' + textSize + ' text-slate-700"><a href="https://doi.org/' + lit.doi + '" target="_blank" class="text-blue-600 hover:underline">' + lit.doi + '</a></p>';
        html += '</div>';
    }
    
    // 源文件详细信息（多文件时显示）
    if (lit.sourceFileName) {
        html += '<div class="mb-4 p-3 bg-blue-50 rounded-lg">';
        html += '<p class="text-xs text-slate-500 mb-1">来源文件</p>';
        html += '<p class="' + textSize + ' text-slate-700">' + lit.sourceFileName + ' (文件 ' + lit.sourceFileIndex + ', 第 ' + lit.indexInFile + ' 条)</p>';
        html += '</div>';
    }
    
    // 错误信息
    if (lit.errorMessage) {
        html += '<div class="p-3 bg-red-50 rounded-lg">';
        html += '<p class="text-sm text-red-600">错误: ' + lit.errorMessage + '</p>';
        html += '</div>';
    }
    
    html += '</div>';
    content.innerHTML = html;
    
    document.getElementById('litNavInfo').textContent = '/ ' + literatures.length;
    document.getElementById('litIndexInput').value = currentLitIndex + 1;
    document.getElementById('litIndexInput').max = literatures.length;
}

function viewLiterature(index) {
    currentLitIndex = index;
    setView('detail');
}

function prevLiterature() {
    if (currentLitIndex > 0) {
        currentLitIndex--;
        renderDetailView();
    }
}

function nextLiterature() {
    if (currentLitIndex < literatures.length - 1) {
        currentLitIndex++;
        renderDetailView();
    }
}

function getStatusClass(status) {
    if (status === 'completed') return 'bg-green-100 text-green-600';
    if (status === 'failed') return 'bg-red-100 text-red-600';
    if (status === 'translating') return 'bg-yellow-100 text-yellow-600';
    return 'bg-gray-100 text-gray-600';
}

function getStatusText(status) {
    if (status === 'completed') return '已完成';
    if (status === 'failed') return '失败';
    if (status === 'translating') return '翻译中';
    return '待处理';
}

function toggleSelect(id) {
    if (selectedIds.has(id)) {
        selectedIds.delete(id);
    } else {
        selectedIds.add(id);
    }
    updateSelection();
    if (currentView === 'list') {
        renderListView();
    } else {
        renderDetailView();
    }
}

function updateSelection() {
    var count = selectedIds.size;
    document.getElementById('selectedCount').textContent = count;
    document.getElementById('exportBtn').disabled = count === 0;
}

function clearSelection() {
    selectedIds.clear();
    updateSelection();
    if (currentView === 'list') {
        renderListView();
    } else {
        renderDetailView();
    }
}

function selectAll() {
    for (var i = 0; i < literatures.length; i++) {
        selectedIds.add(literatures[i].recordNumber);
    }
    updateSelection();
    if (currentView === 'list') {
        renderListView();
    } else {
        renderDetailView();
    }
    showToast('已选择全部 ' + literatures.length + ' 篇文献', 'success');
}

async function pauseTask() {
    try {
        await apiCall('PUT', '/api/tasks/' + taskId + '/pause');
        showToast('任务已暂停', 'success');
        loadTask();
    } catch (error) {
        showToast('暂停失败: ' + error.message, 'error');
    }
}

async function resumeTask() {
    try {
        await apiCall('PUT', '/api/tasks/' + taskId + '/resume');
        showToast('任务已恢复', 'success');
        loadTask();
    } catch (error) {
        showToast('恢复失败: ' + error.message, 'error');
    }
}

async function exportSelected() {
    if (selectedIds.size === 0) {
        showToast('请先选择要导出的文献', 'warning');
        return;
    }
    
    var format = document.querySelector('input[name="exportFormat"]:checked').value;
    
    // PDF导出：在客户端生成
    if (format === 'pdf') {
        exportToPdf();
        return;
    }
    
    try {
        showLoading('正在导出...');
        var response = await fetch('/api/tasks/' + taskId + '/export', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                format: format,
                fields: ['title', 'abstract', 'authors', 'keywords'],
                recordNumbers: Array.from(selectedIds)
            })
        });
        
        if (!response.ok) throw new Error('导出失败');
        
        var blob = await response.blob();
        var url = window.URL.createObjectURL(blob);
        var a = document.createElement('a');
        a.href = url;
        a.download = 'export_' + taskId.replace('/', '_') + '.' + format;
        a.click();
        window.URL.revokeObjectURL(url);
        
        hideLoading();
        showToast('导出成功', 'success');
    } catch (error) {
        hideLoading();
        showToast('导出失败: ' + error.message, 'error');
    }
}

function exportToPdf() {
    var selectedLits = literatures.filter(function(lit) {
        return selectedIds.has(lit.recordNumber);
    });
    
    if (selectedLits.length === 0) {
        showToast('请先选择要导出的文献', 'warning');
        return;
    }
    
    // 构建PDF打印HTML - 每页一篇文献
    var html = '<!DOCTYPE html><html><head><meta charset="UTF-8">';
    html += '<title>文献翻译结果</title>';
    html += '<style>';
    html += '@media print { @page { margin: 2cm; } }';
    html += 'body { font-family: "Microsoft YaHei", "SimSun", Arial, sans-serif; margin: 0; padding: 0; color: #333; }';
    html += '.page { page-break-after: always; padding: 20px; }';
    html += '.page:last-child { page-break-after: auto; }';
    html += '.header { border-bottom: 2px solid #2563eb; padding-bottom: 10px; margin-bottom: 16px; }';
    html += '.record-num { font-size: 13px; color: #6b7280; }';
    html += '.title { font-size: 18px; font-weight: bold; color: #1e40af; margin: 8px 0; line-height: 1.4; }';
    html += '.title-en { font-size: 14px; color: #6b7280; margin: 4px 0 12px 0; line-height: 1.3; }';
    html += '.section { margin-bottom: 14px; }';
    html += '.section-label { font-size: 12px; font-weight: bold; color: #374151; margin-bottom: 4px; }';
    html += '.section-content { font-size: 14px; line-height: 1.7; color: #1f2937; }';
    html += '.section-content-en { font-size: 12px; line-height: 1.5; color: #6b7280; margin-top: 6px; padding: 8px; background: #f9fafb; border-radius: 4px; }';
    html += '.meta { font-size: 12px; color: #6b7280; }';
    html += '.meta-row { display: flex; margin-bottom: 4px; }';
    html += '.meta-label { font-weight: bold; min-width: 80px; color: #374151; }';
    html += '.doi-link { color: #2563eb; text-decoration: none; }';
    html += '</style></head><body>';
    
    for (var i = 0; i < selectedLits.length; i++) {
        var lit = selectedLits[i];
        html += '<div class="page">';
        html += '<div class="header">';
        html += '<div class="record-num">文献 ' + lit.recordNumber + ' / ' + lit.totalRecords + '</div>';
        
        if (lit.translatedTitle) {
            html += '<div class="title">' + escapeHtmlStr(lit.translatedTitle) + '</div>';
        }
        if (lit.originalTitle) {
            html += '<div class="title-en">' + escapeHtmlStr(lit.originalTitle) + '</div>';
        }
        html += '</div>';
        
        // 摘要
        if (lit.translatedAbstract) {
            html += '<div class="section">';
            html += '<div class="section-label">摘要</div>';
            html += '<div class="section-content">' + escapeHtmlStr(lit.translatedAbstract) + '</div>';
            if (lit.originalAbstract) {
                html += '<div class="section-content-en">' + escapeHtmlStr(lit.originalAbstract) + '</div>';
            }
            html += '</div>';
        } else if (lit.originalAbstract) {
            html += '<div class="section">';
            html += '<div class="section-label">摘要 (原文)</div>';
            html += '<div class="section-content">' + escapeHtmlStr(lit.originalAbstract) + '</div>';
            html += '</div>';
        }
        
        // 元数据
        html += '<div class="section meta">';
        if (lit.authors) {
            html += '<div class="meta-row"><span class="meta-label">作者：</span><span>' + escapeHtmlStr(lit.authors) + '</span></div>';
        }
        if (lit.source) {
            var sourceStr = lit.source;
            if (lit.volume) sourceStr += ', Vol. ' + lit.volume;
            if (lit.issue) sourceStr += ', No. ' + lit.issue;
            if (lit.pages) sourceStr += ', pp. ' + lit.pages;
            html += '<div class="meta-row"><span class="meta-label">来源：</span><span>' + escapeHtmlStr(sourceStr) + '</span></div>';
        }
        if (lit.doi) {
            html += '<div class="meta-row"><span class="meta-label">DOI：</span><span><a class="doi-link" href="https://doi.org/' + lit.doi + '">' + escapeHtmlStr(lit.doi) + '</a></span></div>';
        }
        if (lit.publishedDate) {
            html += '<div class="meta-row"><span class="meta-label">发表日期：</span><span>' + escapeHtmlStr(lit.publishedDate) + '</span></div>';
        }
        if (lit.accessionNumber) {
            html += '<div class="meta-row"><span class="meta-label">WoS号：</span><span>' + escapeHtmlStr(lit.accessionNumber) + '</span></div>';
        }
        if (lit.issn) {
            html += '<div class="meta-row"><span class="meta-label">ISSN：</span><span>' + escapeHtmlStr(lit.issn) + '</span></div>';
        }
        if (lit.translatedByModel) {
            html += '<div class="meta-row"><span class="meta-label">翻译模型：</span><span>' + escapeHtmlStr(lit.translatedByModel) + '</span></div>';
        }
        html += '</div>';
        
        html += '</div>';
    }
    
    html += '</body></html>';
    
    // 打开新窗口打印
    var printWindow = window.open('', '_blank');
    printWindow.document.write(html);
    printWindow.document.close();
    printWindow.onload = function() {
        printWindow.print();
    };
}

function escapeHtmlStr(str) {
    if (!str) return '';
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

// ========== 重新翻译功能 ==========

async function retryFailed() {
    if (!task || task.failedCount === 0) {
        showToast('没有失败的文献需要重试', 'warning');
        return;
    }
    
    if (!confirm('确认重试 ' + task.failedCount + ' 篇失败的文献？将使用当前模型配置重新翻译。')) {
        return;
    }
    
    try {
        showLoading('正在重置失败项...');
        var result = await apiCall('POST', '/api/tasks/' + taskId + '/retry-failed');
        hideLoading();
        
        if (result.success) {
            showToast('已重置 ' + result.resetCount + ' 篇失败文献，任务将自动重新开始', 'success');
            loadTask();
        } else {
            showToast('重试失败: ' + (result.error || '未知错误'), 'error');
        }
    } catch (error) {
        hideLoading();
        showToast('重试失败: ' + error.message, 'error');
    }
}

async function retryWithModel() {
    if (!task || task.failedCount === 0) {
        showToast('没有失败的文献需要重试', 'warning');
        return;
    }
    
    // 加载可用模型列表
    try {
        var models = await apiCall('GET', '/api/models');
        if (!models || models.length === 0) {
            showToast('没有可用的模型，请先在模型页面添加模型', 'error');
            return;
        }
        
        showModelSelectModal(models, '选择模型重试失败项', async function(selectedModels) {
            try {
                showLoading('正在重置失败项并更换模型...');
                var result = await apiCall('POST', '/api/tasks/' + taskId + '/retry-failed', {
                    modelConfigs: selectedModels
                });
                hideLoading();
                
                if (result.success) {
                    showToast('已重置 ' + result.resetCount + ' 篇失败文献，将使用新模型重新翻译', 'success');
                    loadTask();
                } else {
                    showToast('操作失败: ' + (result.error || '未知错误'), 'error');
                }
            } catch (error) {
                hideLoading();
                showToast('操作失败: ' + error.message, 'error');
            }
        });
    } catch (error) {
        showToast('加载模型列表失败: ' + error.message, 'error');
    }
}

async function resetTask() {
    if (!task) return;
    
    // 加载可用模型列表
    try {
        var models = await apiCall('GET', '/api/models');
        if (!models || models.length === 0) {
            showToast('没有可用的模型，请先在模型页面添加模型', 'error');
            return;
        }
        
        showModelSelectModal(models, '重置任务 - 选择模型重新翻译全部文献', async function(selectedModels) {
            if (!confirm('确认重置整个任务？所有 ' + task.totalCount + ' 篇文献的翻译结果将被清除，使用新模型重新翻译。')) {
                return;
            }
            
            try {
                showLoading('正在重置任务...');
                var result = await apiCall('POST', '/api/tasks/' + taskId + '/reset', {
                    modelConfigs: selectedModels
                });
                hideLoading();
                
                if (result.success) {
                    showToast('任务已重置，' + result.resetCount + ' 篇文献将重新翻译', 'success');
                    loadTask();
                } else {
                    showToast('重置失败: ' + (result.error || '未知错误'), 'error');
                }
            } catch (error) {
                hideLoading();
                showToast('重置失败: ' + error.message, 'error');
            }
        });
    } catch (error) {
        showToast('加载模型列表失败: ' + error.message, 'error');
    }
}

// 模型选择弹窗
function showModelSelectModal(models, title, onConfirm) {
    // 移除已有弹窗
    var existing = document.getElementById('modelSelectModal');
    if (existing) existing.remove();
    
    var selectedModels = [];
    
    var modal = document.createElement('div');
    modal.id = 'modelSelectModal';
    modal.className = 'fixed inset-0 bg-black/50 flex items-center justify-center z-50';
    
    var html = '<div class="bg-white rounded-xl shadow-2xl w-full max-w-lg mx-4 max-h-[80vh] flex flex-col">';
    html += '<div class="p-4 border-b border-gray-200">';
    html += '<h3 class="text-lg font-semibold text-slate-900">' + escapeHtmlStr(title) + '</h3>';
    html += '<p class="text-sm text-slate-500 mt-1">选择一个或多个模型，设置每个模型的并发线程数</p>';
    html += '</div>';
    
    html += '<div class="p-4 overflow-y-auto flex-1">';
    html += '<div id="modelList" class="space-y-2">';
    
    for (var i = 0; i < models.length; i++) {
        var m = models[i];
        html += '<div class="flex items-center space-x-3 p-3 border border-gray-200 rounded-lg hover:border-blue-300 transition-colors cursor-pointer" data-model-index="' + i + '">';
        html += '<input type="checkbox" class="model-checkbox w-4 h-4 text-blue-600 rounded cursor-pointer" data-index="' + i + '">';
        html += '<div class="flex-1 min-w-0">';
        html += '<p class="text-sm font-medium text-slate-900 truncate">' + escapeHtmlStr(m.name || m.modelId) + '</p>';
        html += '<p class="text-xs text-slate-500 truncate">' + escapeHtmlStr(m.modelId) + '</p>';
        html += '</div>';
        html += '<div class="flex items-center space-x-1 flex-shrink-0">';
        html += '<label class="text-xs text-slate-500">线程</label>';
        html += '<input type="number" class="model-threads w-14 px-2 py-1 text-xs border border-gray-300 rounded text-center" data-index="' + i + '" value="1" min="1" max="10">';
        html += '</div>';
        html += '</div>';
    }
    
    html += '</div>';
    html += '</div>';
    
    html += '<div class="p-4 border-t border-gray-200 flex items-center justify-between">';
    html += '<span id="modelSelectCount" class="text-sm text-slate-500">已选 0 个模型</span>';
    html += '<div class="flex items-center space-x-2">';
    html += '<button id="modelSelectCancel" class="px-4 py-2 text-sm text-slate-600 hover:bg-slate-100 rounded-lg transition-colors cursor-pointer">取消</button>';
    html += '<button id="modelSelectConfirm" class="px-4 py-2 text-sm bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors cursor-pointer disabled:bg-gray-300 disabled:cursor-not-allowed" disabled>确认</button>';
    html += '</div>';
    html += '</div>';
    html += '</div>';
    
    modal.innerHTML = html;
    document.body.appendChild(modal);
    
    // 点击行切换checkbox
    var modelItems = modal.querySelectorAll('[data-model-index]');
    modelItems.forEach(function(item) {
        item.addEventListener('click', function(e) {
            if (e.target.tagName === 'INPUT') return;
            var cb = item.querySelector('.model-checkbox');
            cb.checked = !cb.checked;
            cb.dispatchEvent(new Event('change'));
        });
    });
    
    // checkbox变化
    function updateModelCount() {
        var checked = modal.querySelectorAll('.model-checkbox:checked');
        document.getElementById('modelSelectCount').textContent = '已选 ' + checked.length + ' 个模型';
        document.getElementById('modelSelectConfirm').disabled = checked.length === 0;
    }
    
    modal.querySelectorAll('.model-checkbox').forEach(function(cb) {
        cb.addEventListener('change', updateModelCount);
    });
    
    // 取消
    document.getElementById('modelSelectCancel').addEventListener('click', function() {
        modal.remove();
    });
    
    // 点击背景关闭
    modal.addEventListener('click', function(e) {
        if (e.target === modal) modal.remove();
    });
    
    // 确认
    document.getElementById('modelSelectConfirm').addEventListener('click', function() {
        var result = [];
        modal.querySelectorAll('.model-checkbox:checked').forEach(function(cb) {
            var idx = parseInt(cb.dataset.index);
            var m = models[idx];
            var threads = parseInt(modal.querySelector('.model-threads[data-index="' + idx + '"]').value) || 1;
            result.push({
                url: m.url,
                apiKey: m.apiKey,
                modelId: m.modelId,
                name: m.name || m.modelId,
                temperature: m.temperature || 0.7,
                systemPrompt: m.systemPrompt || '',
                provider: m.provider || 'openai',
                enableThinking: m.enableThinking || false,
                autoAppendPath: m.autoAppendPath !== undefined ? m.autoAppendPath : true,
                threads: threads
            });
        });
        
        modal.remove();
        onConfirm(result);
    });
}
