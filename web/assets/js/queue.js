let tasks = [];
let refreshInterval = null;
let currentLayout = localStorage.getItem('queueLayout') || 'list';
let currentDateFilter = null;
let currentSearchQuery = '';
let calendarYear = 0;
let calendarMonth = 0;
let calendarOpen = false;

document.addEventListener('DOMContentLoaded', function() {
    var today = new Date();
    calendarYear = today.getFullYear();
    calendarMonth = today.getMonth();
    currentDateFilter = formatDate(today);
    updateCalendarLabel();
    loadTasks();
    setupEventListeners();
    updateLayoutButtons();
    refreshInterval = setInterval(loadTasks, 3000);
});

window.addEventListener('beforeunload', function() {
    if (refreshInterval) clearInterval(refreshInterval);
});

function formatDate(d) {
    return d.getFullYear() + '-' +
        String(d.getMonth() + 1).padStart(2, '0') + '-' +
        String(d.getDate()).padStart(2, '0');
}

function updateCalendarLabel() {
    var label = document.getElementById('calendarLabel');
    if (currentDateFilter) {
        // 显示简短日期 如 "02-07"
        label.textContent = currentDateFilter.substring(5);
    } else {
        label.textContent = '全部日期';
    }
}

function setupEventListeners() {
    document.getElementById('refreshTasks').addEventListener('click', loadTasks);
    document.getElementById('listViewBtn').addEventListener('click', function() { setLayout('list'); });
    document.getElementById('gridViewBtn').addEventListener('click', function() { setLayout('grid'); });

    // 日历弹窗开关
    document.getElementById('calendarToggle').addEventListener('click', function(e) {
        e.stopPropagation();
        toggleCalendar();
    });

    // 点击外部关闭日历
    document.addEventListener('click', function(e) {
        var dropdown = document.getElementById('calendarDropdown');
        if (calendarOpen && !dropdown.contains(e.target)) {
            closeCalendar();
        }
    });

    // 日历内部点击不冒泡关闭
    document.getElementById('calendarDropdown').addEventListener('click', function(e) {
        e.stopPropagation();
    });

    // 日历滚轮切换月份
    document.getElementById('calendarDropdown').addEventListener('wheel', function(e) {
        e.preventDefault();
        if (e.deltaY > 0) {
            calendarMonth++;
            if (calendarMonth > 11) { calendarMonth = 0; calendarYear++; }
        } else {
            calendarMonth--;
            if (calendarMonth < 0) { calendarMonth = 11; calendarYear--; }
        }
        renderCalendar();
    });

    // 日历导航
    document.getElementById('prevMonth').addEventListener('click', function() {
        calendarMonth--;
        if (calendarMonth < 0) { calendarMonth = 11; calendarYear--; }
        renderCalendar();
    });
    document.getElementById('nextMonth').addEventListener('click', function() {
        calendarMonth++;
        if (calendarMonth > 11) { calendarMonth = 0; calendarYear++; }
        renderCalendar();
    });
    document.getElementById('todayBtn').addEventListener('click', function() {
        var today = new Date();
        calendarYear = today.getFullYear();
        calendarMonth = today.getMonth();
        currentDateFilter = formatDate(today);
        updateCalendarLabel();
        renderCalendar();
        renderTasks();
        closeCalendar();
    });
    document.getElementById('showAllBtn').addEventListener('click', function() {
        currentDateFilter = null;
        updateCalendarLabel();
        renderCalendar();
        renderTasks();
        closeCalendar();
    });

    // 搜索 - 在全部任务中搜索
    var searchInput = document.getElementById('searchInput');
    var searchTimer = null;
    searchInput.addEventListener('input', function() {
        clearTimeout(searchTimer);
        searchTimer = setTimeout(function() {
            currentSearchQuery = searchInput.value.trim().toLowerCase();
            document.getElementById('clearSearch').classList.toggle('hidden', !currentSearchQuery);
            renderTasks();
        }, 200);
    });
    document.getElementById('clearSearch').addEventListener('click', function() {
        searchInput.value = '';
        currentSearchQuery = '';
        this.classList.add('hidden');
        renderTasks();
    });
}

function toggleCalendar() {
    if (calendarOpen) {
        closeCalendar();
    } else {
        openCalendar();
    }
}

function openCalendar() {
    calendarOpen = true;
    document.getElementById('calendarDropdown').classList.remove('hidden');
    renderCalendar();
}

function closeCalendar() {
    calendarOpen = false;
    document.getElementById('calendarDropdown').classList.add('hidden');
}

function setLayout(layout) {
    currentLayout = layout;
    localStorage.setItem('queueLayout', layout);
    updateLayoutButtons();
    renderTasks();
}

function updateLayoutButtons() {
    var listBtn = document.getElementById('listViewBtn');
    var gridBtn = document.getElementById('gridViewBtn');
    if (currentLayout === 'list') {
        listBtn.classList.add('bg-blue-100', 'text-blue-600');
        listBtn.classList.remove('text-slate-600');
        gridBtn.classList.remove('bg-blue-100', 'text-blue-600');
        gridBtn.classList.add('text-slate-600');
    } else {
        gridBtn.classList.add('bg-blue-100', 'text-blue-600');
        gridBtn.classList.remove('text-slate-600');
        listBtn.classList.remove('bg-blue-100', 'text-blue-600');
        listBtn.classList.add('text-slate-600');
    }
}

// ========== 日历 ==========

function getTaskCountsByDate() {
    var counts = {};
    for (var i = 0; i < tasks.length; i++) {
        var date = tasks[i].taskId.substring(0, 10);
        counts[date] = (counts[date] || 0) + 1;
    }
    return counts;
}

function renderCalendar() {
    var title = calendarYear + '年' + (calendarMonth + 1) + '月';
    document.getElementById('calendarTitle').textContent = title;

    var counts = getTaskCountsByDate();
    var today = formatDate(new Date());

    var firstDay = new Date(calendarYear, calendarMonth, 1);
    var lastDay = new Date(calendarYear, calendarMonth + 1, 0);
    var startDow = firstDay.getDay();
    var daysInMonth = lastDay.getDate();

    var grid = document.getElementById('calendarGrid');
    var html = '';

    for (var i = 0; i < startDow; i++) {
        html += '<div class="py-1"></div>';
    }

    for (var d = 1; d <= daysInMonth; d++) {
        var dateStr = calendarYear + '-' + String(calendarMonth + 1).padStart(2, '0') + '-' + String(d).padStart(2, '0');
        var count = counts[dateStr] || 0;
        var isToday = dateStr === today;
        var isSelected = dateStr === currentDateFilter;

        var cellClass = 'relative py-1 rounded-lg cursor-pointer transition-all text-xs ';
        if (isSelected) {
            cellClass += 'bg-blue-600 text-white font-semibold';
        } else if (isToday) {
            cellClass += 'bg-blue-50 text-blue-600 font-semibold hover:bg-blue-100';
        } else if (count > 0) {
            cellClass += 'text-slate-900 hover:bg-slate-100 font-medium';
        } else {
            cellClass += 'text-slate-400 hover:bg-slate-50';
        }

        html += '<div class="' + cellClass + '" onclick="selectDate(\'' + dateStr + '\')">';
        html += '<div>' + d + '</div>';
        if (count > 0) {
            html += '<div class="flex justify-center mt-0.5"><span class="inline-block text-center leading-none rounded-full px-1 ' +
                (isSelected ? 'text-blue-200 text-[9px]' : 'text-blue-600 text-[9px]') + '">' + count + '</span></div>';
        }
        html += '</div>';
    }

    grid.innerHTML = html;
}

function selectDate(dateStr) {
    if (currentDateFilter === dateStr) {
        currentDateFilter = null;
    } else {
        currentDateFilter = dateStr;
    }
    updateCalendarLabel();
    renderCalendar();
    renderTasks();
    closeCalendar();
}

function clearDateFilterAndShow() {
    currentDateFilter = null;
    updateCalendarLabel();
    renderCalendar();
    renderTasks();
}

// ========== 任务加载和渲染 ==========

async function loadTasks() {
    try {
        var response = await apiCall('GET', '/api/tasks');
        tasks = response;
        if (calendarOpen) renderCalendar();
        renderTasks();
    } catch (error) {
        document.getElementById('taskList').innerHTML = '<div class="text-center py-12 text-red-500">加载失败: ' + error.message + '</div>';
    }
}

function renderTasks() {
    var taskList = document.getElementById('taskList');
    var filterInfo = document.getElementById('filterInfo');

    var filteredTasks = tasks;
    var filters = [];

    // 搜索在全部任务中进行（不受日期限制）
    if (currentSearchQuery) {
        filteredTasks = filteredTasks.filter(function(task) {
            var name = (task.taskName || '').toLowerCase();
            var file = (task.fileName || '').toLowerCase();
            var id = (task.taskId || '').toLowerCase();
            var model = (task.modelName || '').toLowerCase();
            return name.indexOf(currentSearchQuery) !== -1 ||
                   file.indexOf(currentSearchQuery) !== -1 ||
                   id.indexOf(currentSearchQuery) !== -1 ||
                   model.indexOf(currentSearchQuery) !== -1;
        });
        filters.push('搜索: "' + currentSearchQuery + '"');
    } else if (currentDateFilter) {
        // 只在没有搜索时才按日期过滤
        filteredTasks = filteredTasks.filter(function(task) {
            return task.taskId.startsWith(currentDateFilter);
        });
        filters.push(currentDateFilter);
    }

    // 显示过滤信息
    if (filters.length > 0 || filteredTasks.length !== tasks.length) {
        filterInfo.textContent = (filters.length > 0 ? filters.join(' · ') + ' · ' : '') + '共 ' + filteredTasks.length + ' 个任务';
        filterInfo.classList.remove('hidden');
    } else {
        filterInfo.textContent = '共 ' + filteredTasks.length + ' 个任务';
        filterInfo.classList.remove('hidden');
    }

    if (filteredTasks.length === 0) {
        var message = (currentDateFilter || currentSearchQuery)
            ? '没有匹配的任务，<button onclick="clearAllFilters()" class="text-blue-600 hover:underline cursor-pointer">清除筛选</button> 或 <a href="/translate.html" class="text-blue-600 hover:underline cursor-pointer">创建新任务</a>'
            : '暂无任务，<a href="/translate.html" class="text-blue-600 hover:underline cursor-pointer">创建新任务</a>';
        taskList.innerHTML = '<div class="text-center py-12 text-slate-500">' + message + '</div>';
        return;
    }
    if (currentLayout === 'grid') {
        taskList.className = 'grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4';
        taskList.innerHTML = filteredTasks.map(function(task) { return renderGridCard(task); }).join('');
    } else {
        taskList.className = 'space-y-2';
        taskList.innerHTML = filteredTasks.map(function(task) { return renderListCard(task); }).join('');
    }
}

function clearAllFilters() {
    currentDateFilter = null;
    currentSearchQuery = '';
    document.getElementById('searchInput').value = '';
    document.getElementById('clearSearch').classList.add('hidden');
    updateCalendarLabel();
    if (calendarOpen) renderCalendar();
    renderTasks();
}

function renderListCard(task) {
    var status = getTaskStatusDisplay(task.status);
    var progress = task.totalCount > 0 ? (task.completedCount / task.totalCount * 100).toFixed(0) : 0;
    var displayName = task.taskName || task.fileName;
    var html = '<div class="bg-white rounded-lg px-4 py-3 shadow-sm border border-gray-200 hover:border-blue-300 transition-colors cursor-pointer" onclick="goToTask(\'' + task.taskId + '\')">';
    html += '<div class="flex items-center justify-between">';
    html += '<div class="flex items-center space-x-4 flex-1 min-w-0">';
    html += '<span class="px-2 py-0.5 ' + status.bg + ' ' + status.color + ' text-xs font-medium rounded-full flex-shrink-0">' + status.text + '</span>';
    html += '<div class="flex-1 min-w-0"><p class="text-sm font-medium text-slate-900 truncate">' + displayName + '</p>';
    html += '<p class="text-xs text-slate-500">' + task.taskId + (task.taskName ? ' · ' + task.fileName : '') + '</p></div>';
    html += '<div class="flex items-center space-x-3 flex-shrink-0"><div class="w-32">';
    html += '<div class="flex items-center justify-between text-xs text-slate-500 mb-1"><span>' + task.completedCount + '/' + task.totalCount + '</span><span>' + progress + '%</span></div>';
    html += '<div class="w-full bg-gray-200 rounded-full h-1.5"><div class="bg-blue-600 h-1.5 rounded-full" style="width:' + progress + '%"></div></div></div>';
    if (task.failedCount > 0) html += '<span class="text-xs text-red-600">失败' + task.failedCount + '</span>';
    html += '</div></div>';
    html += '<div class="flex items-center space-x-1 ml-4" onclick="event.stopPropagation()">' + renderActionButtons(task) + '</div>';
    html += '</div></div>';
    return html;
}

function renderGridCard(task) {
    var status = getTaskStatusDisplay(task.status);
    var progress = task.totalCount > 0 ? (task.completedCount / task.totalCount * 100).toFixed(0) : 0;
    var displayName = task.taskName || task.fileName;
    var html = '<div class="bg-white rounded-xl p-4 shadow-sm border border-gray-200 hover:border-blue-300 hover:shadow-md transition-all cursor-pointer" onclick="goToTask(\'' + task.taskId + '\')">';
    html += '<div class="flex items-start justify-between mb-3">';
    html += '<span class="px-2 py-0.5 ' + status.bg + ' ' + status.color + ' text-xs font-medium rounded-full">' + status.text + '</span>';
    if (task.failedCount > 0) html += '<span class="text-xs text-red-600">失败' + task.failedCount + '</span>';
    html += '</div>';
    html += '<h3 class="text-sm font-medium text-slate-900 mb-1 truncate" title="' + displayName + '">' + displayName + '</h3>';
    html += '<p class="text-xs text-slate-500 mb-3">' + task.taskId + (task.taskName ? ' · ' + task.fileName : '') + '</p>';
    html += '<div class="mb-3"><div class="flex items-center justify-between text-xs text-slate-500 mb-1"><span>' + task.completedCount + '/' + task.totalCount + '</span><span>' + progress + '%</span></div>';
    html += '<div class="w-full bg-gray-200 rounded-full h-1.5"><div class="bg-blue-600 h-1.5 rounded-full" style="width:' + progress + '%"></div></div></div>';
    html += '<div class="flex items-center justify-between pt-2 border-t border-gray-100">';
    if (task.modelCount > 1) {
        html += '<span class="text-xs text-slate-500 truncate max-w-[100px]" title="' + task.modelCount + '个模型">';
        html += '<svg class="w-3 h-3 inline mr-1" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z"></path></svg>';
        html += task.modelCount + '个模型</span>';
    } else if (task.modelName) {
        html += '<span class="text-xs text-slate-500 truncate max-w-[100px]" title="' + task.modelName + '">';
        html += '<svg class="w-3 h-3 inline mr-1" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z"></path></svg>';
        html += task.modelName + '</span>';
    } else {
        html += '<span></span>';
    }
    html += '<div class="flex items-center space-x-1" onclick="event.stopPropagation()">' + renderActionButtons(task) + '</div>';
    html += '</div></div>';
    return html;
}

function renderActionButtons(task) {
    var btnClass = 'p-1.5 rounded hover:bg-slate-100 transition-colors cursor-pointer';
    var html = '';
    if (task.status === 4) {
        html += '<button onclick="exportTaskHtml(\'' + task.taskId + '\')" class="' + btnClass + ' text-blue-600 hover:bg-blue-50" title="导出HTML"><svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 10v6m0 0l-3-3m3 3l3-3m2 8H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z"></path></svg></button>';
    } else {
        html += '<button disabled class="' + btnClass + ' text-gray-300 cursor-not-allowed" title="任务完成后可导出"><svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 10v6m0 0l-3-3m3 3l3-3m2 8H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z"></path></svg></button>';
    }
    if (task.status === 2) {
        html += '<button onclick="pauseTask(\'' + task.taskId + '\')" class="' + btnClass + ' text-orange-600 hover:bg-orange-50" title="暂停"><svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10 9v6m4-6v6m7-3a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg></button>';
    }
    if (task.status === 3) {
        html += '<button onclick="resumeTask(\'' + task.taskId + '\')" class="' + btnClass + ' text-green-600 hover:bg-green-50" title="恢复"><svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z"></path><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path></svg></button>';
    }
    html += '<button onclick="deleteTask(\'' + task.taskId + '\')" class="' + btnClass + ' text-red-600 hover:bg-red-50" title="删除"><svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path></svg></button>';
    return html;
}

function goToTask(taskId) {
    window.location.href = '/task.html?id=' + encodeURIComponent(taskId);
}

async function pauseTask(taskId) {
    try {
        await apiCall('PUT', '/api/tasks/' + taskId + '/pause');
        showToast('任务已暂停', 'success');
        loadTasks();
    } catch (error) {
        showToast('暂停失败: ' + error.message, 'error');
    }
}

async function resumeTask(taskId) {
    try {
        await apiCall('PUT', '/api/tasks/' + taskId + '/resume');
        showToast('任务已恢复', 'success');
        loadTasks();
    } catch (error) {
        showToast('恢复失败: ' + error.message, 'error');
    }
}

function deleteTask(taskId) {
    showConfirm('确定要删除这个任务吗？此操作不可恢复。', async function() {
        try {
            await apiCall('DELETE', '/api/tasks/' + taskId);
            showToast('任务已删除', 'success');
            loadTasks();
        } catch (error) {
            showToast('删除失败: ' + error.message, 'error');
        }
    });
}

async function exportTaskHtml(taskId) {
    try {
        showLoading('正在导出...');
        var response = await fetch('/api/tasks/' + taskId + '/export', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                format: 'html',
                fields: ['title', 'abstract', 'authors', 'keywords'],
                recordNumbers: []
            })
        });
        if (!response.ok) throw new Error('导出失败');
        var blob = await response.blob();
        var url = window.URL.createObjectURL(blob);
        var a = document.createElement('a');
        a.href = url;
        a.download = 'export_' + taskId.replace('/', '_') + '.html';
        a.click();
        window.URL.revokeObjectURL(url);
        hideLoading();
        showToast('导出成功', 'success');
    } catch (error) {
        hideLoading();
        showToast('导出失败: ' + error.message, 'error');
    }
}
