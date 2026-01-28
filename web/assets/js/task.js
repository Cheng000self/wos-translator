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
    document.getElementById('clearSelectionBtn').addEventListener('click', clearSelection);
    document.getElementById('pauseBtn').addEventListener('click', pauseTask);
    document.getElementById('resumeBtn').addEventListener('click', resumeTask);
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
    pauseBtn.classList.add('hidden');
    resumeBtn.classList.add('hidden');
    
    if (task.status === 2) {
        pauseBtn.classList.remove('hidden');
    } else if (task.status === 3) {
        resumeBtn.classList.remove('hidden');
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
